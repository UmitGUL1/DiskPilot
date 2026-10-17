#include "diskpilot/duplicates.hpp"
#include "diskpilot/file_access.hpp"
#include <algorithm>
#include <bit>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace diskpilot {
void Sha256::block() {
    static constexpr std::array<std::uint32_t,64> k{
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    std::array<std::uint32_t,64> w{};
    for (unsigned i=0;i<16;++i) for(unsigned j=0;j<4;++j) w[i]=(w[i]<<8U)|std::to_integer<std::uint32_t>(buffer_[i*4+j]);
    for(unsigned i=16;i<64;++i) {
        const auto a=w[i-15], b=w[i-2];
        w[i]=w[i-16]+(std::rotr(a,7)^std::rotr(a,18)^(a>>3U))+w[i-7]+(std::rotr(b,17)^std::rotr(b,19)^(b>>10U));
    }
    auto [a,b,c,d,e,f,g,h]=state_;
    for(unsigned i=0;i<64;++i) {
        const auto t1=h+(std::rotr(e,6)^std::rotr(e,11)^std::rotr(e,25))+((e&f)^((~e)&g))+k[i]+w[i];
        const auto t2=(std::rotr(a,2)^std::rotr(a,13)^std::rotr(a,22))+((a&b)^(a&c)^(b&c));
        h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    state_[0]+=a;state_[1]+=b;state_[2]+=c;state_[3]+=d;state_[4]+=e;state_[5]+=f;state_[6]+=g;state_[7]+=h;
}
void Sha256::update(std::span<const std::byte> bytes) {
    if(finished_) throw std::logic_error("SHA-256 already finalized");
    if(bytes.size() > (std::numeric_limits<std::uint64_t>::max()/8U)-bytes_) throw std::length_error("SHA-256 input too large");
    bytes_+=bytes.size();
    for(auto b:bytes) { buffer_[used_++]=b; if(used_==64) {block();used_=0;} }
}
std::string Sha256::finish() {
    if(finished_) throw std::logic_error("SHA-256 already finalized");
    const auto bits=bytes_*8U;
    buffer_[used_++]=std::byte{0x80};
    if(used_>56) {std::fill(buffer_.begin()+static_cast<std::ptrdiff_t>(used_),buffer_.end(),std::byte{});block();used_=0;}
    std::fill(buffer_.begin()+static_cast<std::ptrdiff_t>(used_),buffer_.begin()+56,std::byte{});
    for(unsigned i=0;i<8;++i) buffer_[63-i]=static_cast<std::byte>((bits>>(8U*i))&0xffU);
    block();finished_=true;
    std::ostringstream out;out<<std::hex<<std::setfill('0');
    for(auto word:state_) out<<std::setw(8)<<word;
    return out.str();
}
namespace {
struct Cancelled {};
void checkpoint(std::stop_token stop) { if(stop.stop_requested()) throw Cancelled{}; }
std::string digest(const FileNode& node, bool partial, std::stop_token stop, DuplicateProgress& progress, const DuplicateCallback& cb) {
    CheckedFile file(node);
    Sha256 hash;
    std::array<std::byte,65536> buffer{};
    std::uintmax_t remaining=node.logical_bytes;
    if(partial) remaining=std::min<std::uintmax_t>(remaining,4096);
    auto consume=[&](std::uintmax_t count) {
        while(count) {
            checkpoint(stop);
            const auto wanted=static_cast<std::size_t>(std::min<std::uintmax_t>(count,buffer.size()));
            const auto got=file.read(std::span(buffer).first(wanted));
            if(!got) throw std::runtime_error("Unexpected end of file");
            hash.update(std::span(buffer).first(got));count-=got;progress.bytes_read+=got;
            if(cb) cb(progress);
        }
    };
    consume(remaining);
    if(partial && node.logical_bytes>4096) {
        const auto tail=std::min<std::uintmax_t>(4096,node.logical_bytes-4096);
        file.seek(node.logical_bytes-tail);consume(tail);
    }
    file.verify();checkpoint(stop);
    return hash.finish();
}
bool equals(const FileNode& a,const FileNode& b,std::stop_token stop,DuplicateProgress& progress,const DuplicateCallback& cb) {
    CheckedFile left(a),right(b);
    std::array<std::byte,65536> x{},y{};
    auto remaining=a.logical_bytes;
    while(remaining) {
        checkpoint(stop);
        const auto count=static_cast<std::size_t>(std::min<std::uintmax_t>(remaining,x.size()));
        const auto nx=left.read(std::span(x).first(count)),ny=right.read(std::span(y).first(count));
        if(nx!=count||ny!=count) throw std::runtime_error("File changed during comparison");
        progress.bytes_read+=nx+ny;
        if(cb) cb(progress);
        if(!std::equal(x.begin(),x.begin()+static_cast<std::ptrdiff_t>(nx),y.begin())) {left.verify();right.verify();return false;}
        remaining-=nx;
    }
    left.verify();right.verify();checkpoint(stop);return true;
}
}
DuplicateResult DuplicateDetector::find(const ScanResult& scan,std::stop_token stop,DuplicateCallback cb) const {
    DuplicateResult out;
    std::map<std::uintmax_t,std::vector<std::size_t>> sizes;
    std::unordered_set<std::string> identities;
    auto error=[&](std::size_t i,const std::exception& e) {
        ++out.error_count;
        if(out.errors.size()<1000) out.errors.push_back({scan.nodes[i].path,"duplicate verification",e.what(),false});
    };
    try {
        for(std::size_t i=0;i<scan.nodes.size();++i) {
            checkpoint(stop);const auto& n=scan.nodes[i];
            if(n.kind==NodeKind::file&&!n.identity.empty()&&identities.insert(n.identity).second) sizes[n.logical_bytes].push_back(i);
        }
        for(auto& [size,nodes]:sizes) {
            if(nodes.size()<2) continue;
            std::sort(nodes.begin(),nodes.end(),[&](auto a,auto b){return scan.nodes[a].path.native()<scan.nodes[b].path.native();});
            std::map<std::string,std::vector<std::size_t>> partials;
            for(auto i:nodes) try {checkpoint(stop);partials[digest(scan.nodes[i],true,stop,out.progress,cb)].push_back(i);} catch(const std::exception& e){error(i,e);}
            for(const auto& [partial, candidates]:partials) {
                (void)partial;if(candidates.size()<2) continue;
                std::map<std::string,std::vector<std::size_t>> full;
                for(auto i:candidates) try {
                    checkpoint(stop);full[digest(scan.nodes[i],false,stop,out.progress,cb)].push_back(i);++out.progress.files_hashed;
                } catch(const std::exception& e){error(i,e);}
                for(const auto& [hash,matches]:full) {
                    if(matches.size()<2) continue;
                    std::vector<DuplicateGroup> verified;
                    for(auto i:matches) {
                        bool placed=false;
                        try {
                            checkpoint(stop);
                            for(auto& g:verified) if(equals(scan.nodes[g.nodes[0]],scan.nodes[i],stop,out.progress,cb)) {g.nodes.push_back(i);placed=true;break;}
                            if(!placed) verified.push_back({hash,size,{i}});
                        } catch(const std::exception& e){error(i,e);}
                    }
                    for(auto& g:verified) if(g.nodes.size()>1) {
                        bool stable=true;
                        for(auto i:g.nodes)try{CheckedFile file(scan.nodes[i]);file.verify();}catch(const std::exception& e){error(i,e);stable=false;}
                        if(stable)out.groups.push_back(std::move(g));
                    }
                }
            }
        }
    } catch(const Cancelled&) { out.cancelled=true; }
    // A cancelled operation never publishes previously verified groups as current.
    if(out.cancelled) out.groups.clear();
    out.complete=scan.complete&&!out.cancelled&&out.error_count==0;
    if(cb) cb(out.progress);
    return out;
}
} // namespace diskpilot
