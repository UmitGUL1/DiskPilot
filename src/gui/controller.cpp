#include "controller.hpp"
#include "diskpilot/treemap.hpp"
#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QPointer>
#include <QSettings>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace {
QString text(const diskpilot::fs::path& p) {return QString::fromUtf8(diskpilot::path_utf8(p));}
diskpilot::fs::path path(const QString& s) {
#ifdef _WIN32
    return diskpilot::fs::path(s.toStdWString());
#else
    const auto bytes=s.toUtf8();
    return diskpilot::fs::path(std::u8string(reinterpret_cast<const char8_t*>(bytes.constData()),static_cast<std::size_t>(bytes.size())));
#endif
}
QString bytes(std::uintmax_t n) {
    double size=static_cast<double>(n);int unit=0;
    const QStringList units{"B","KiB","MiB","GiB","TiB","PiB"};
    while(size>=1024&&unit<units.size()-1){size/=1024;++unit;}
    return QString::number(size,'f',unit?2:0)+" "+units[unit];
}
template<class Fn> void post(QPointer<Controller> self,Fn action) {
    if(self)QMetaObject::invokeMethod(self,[self,action=std::move(action)]() mutable {if(self)action(*self);},Qt::QueuedConnection);
}
}
Controller::Controller(QObject* parent):QObject(parent) {
    QSettings settings;target_=settings.value("target").toString();
    entryLimit_=std::clamp(settings.value("entryLimit",2'000'000).toInt(),100,2'000'000);
}
Controller::~Controller(){worker_.request_stop();if(worker_.joinable())worker_.join();}
void Controller::setTarget(const QString& s){if(busy_)return;target_=s;QSettings{}.setValue("target",s);emit stateChanged();}
void Controller::chooseTarget(const QUrl& u){if(u.isLocalFile())setTarget(u.toLocalFile());}
void Controller::setEntryLimit(int n){if(busy_)return;entryLimit_=std::clamp(n,100,2'000'000);QSettings{}.setValue("entryLimit",entryLimit_);emit stateChanged();}
void Controller::message(const QString& s){status_=s;emit progressChanged();}
void Controller::begin(){busy_=true;emit stateChanged();}
void Controller::scan() {
    if(busy_||target_.isEmpty())return;
    const auto root=path(target_);diskpilot::ScanOptions options;options.max_entries=static_cast<std::size_t>(entryLimit_);
    begin();message("Scanning metadata…");QPointer<Controller> self(this);
    worker_=std::jthread([self,root,options](std::stop_token stop){
        try {
            auto result=std::make_shared<diskpilot::ScanResult>(diskpilot::FileSystemScanner{}.scan(root,options,stop,[self](const auto& p){
                post(self,[p](auto& c){c.message(QString("Scanned %1 files · %2 folders · %3").arg(p.files).arg(p.directories).arg(bytes(p.logical_bytes)));});
            }));
            auto cleanup=diskpilot::detect_cleanup(*result);
            auto analysis=diskpilot::analyze(*result);
            post(self,[result,cleanup=std::move(cleanup),analysis=std::move(analysis)](auto& c) mutable {
                c.scan_=result;c.analysis_=std::move(analysis);c.cleanup_=std::move(cleanup);c.duplicates_={};c.selectedCleanup_.clear();c.selectedDuplicates_.clear();c.plan_.clear();c.comparison_.clear();c.directory_=0;c.busy_=false;
                c.message(result->cancelled?"Scan cancelled — partial results.":result->complete?"Scan complete within link and mount policy.":"Partial scan — review access errors or entry limit.");emit c.stateChanged();
            });
        }catch(const std::exception& e){const auto msg=QString::fromUtf8(e.what());post(self,[msg](auto& c){c.busy_=false;c.message(msg);emit c.stateChanged();});}
    });
}
void Controller::cancel(){worker_.request_stop();message("Cancellation requested…");}
QString Controller::currentPath()const{return hasScan()?text(scan_->nodes.at(directory_).path):QString{};}
QVariantMap Controller::summary() const {
    if(!hasScan())return {};
    const auto& s=*scan_;const auto& analysis=analysis_;
    QVariantMap m{{"root",text(s.root)},{"files",QVariant::fromValue<qulonglong>(s.progress.files)},
        {"directories",QVariant::fromValue<qulonglong>(s.progress.directories)},{"logical",bytes(s.progress.logical_bytes)},
        {"allocated",s.nodes[0].allocated_complete?bytes(s.nodes[0].unique_allocated_bytes):"Incomplete metadata"},
        {"complete",s.complete},{"duration",QString::number(s.duration.count())+" ms"},
        {"errors",QVariant::fromValue<qulonglong>(s.error_count)},{"accessDenied",QVariant::fromValue<qulonglong>(s.access_denied_count)},
        {"links",QVariant::fromValue<qulonglong>(s.progress.skipped_links)},{"duplicateComplete",duplicates_.complete}};
    if(analysis.volume){m["capacity"]=bytes(analysis.volume->capacity);m["free"]=bytes(analysis.volume->free);m["used"]=bytes(analysis.volume->capacity-analysis.volume->free);}
    QStringList errors;for(const auto& e:s.errors)errors<<text(e.path)+": "+QString::fromStdString(e.message);
    m["errorDetails"]=errors.join('\n');return m;
}
QVariantMap Controller::row(std::size_t i)const{
    const auto& n=scan_->nodes.at(i);const auto parent=n.parent==diskpilot::no_parent?0:scan_->nodes.at(n.parent).logical_bytes;
    return {{"node",static_cast<int>(i)},{"name",text(n.path.filename())},{"path",text(n.path)},
        {"size",bytes(n.logical_bytes)},{"rawSize",static_cast<double>(n.logical_bytes)},
        {"allocated",n.allocated_complete?bytes(n.unique_allocated_bytes):"Unknown"},
        {"directory",n.kind==diskpilot::NodeKind::directory},{"files",QVariant::fromValue<qulonglong>(n.file_count)},
        {"modified",n.modified_at_unix_ms?QVariant::fromValue<qlonglong>(*n.modified_at_unix_ms):QVariant{}},
        {"percent",parent?100.0*static_cast<double>(n.logical_bytes)/static_cast<double>(parent):0.0}};
}
QVariantList Controller::rows()const{
    QVariantList list;if(!hasScan())return list;
    std::vector<std::size_t> ids;
    for(std::size_t i=1;i<scan_->nodes.size();++i)if(scan_->nodes[i].parent==directory_&&text(scan_->nodes[i].path.filename()).contains(filter_,Qt::CaseInsensitive)&&scan_->nodes[i].logical_bytes>=minimum_)ids.push_back(i);
    const auto count=std::min<std::size_t>(2000,ids.size());
    std::partial_sort(ids.begin(),ids.begin()+static_cast<std::ptrdiff_t>(count),ids.end(),[&](auto a,auto b){return scan_->nodes[a].logical_bytes!=scan_->nodes[b].logical_bytes?scan_->nodes[a].logical_bytes>scan_->nodes[b].logical_bytes:scan_->nodes[a].path.native()<scan_->nodes[b].path.native();});
    for(std::size_t k=0;k<count;++k)list<<row(ids[k]);return list;
}
QVariantList Controller::largest()const{
    QVariantList list;if(!hasScan())return list;
    diskpilot::AnalysisOptions options;options.top=1000;options.minimum_bytes=minimum_;
    // QString filtering handles Unicode case folding before the bounded top-K.
    options.name_contains=filter_.toUtf8().toStdString();
    for(auto i:diskpilot::analyze(*scan_,options).largest_files)list<<row(i);return list;
}
QVariantList Controller::categories()const{
    QVariantList list;if(!hasScan())return list;
    const auto& a=analysis_;
    std::vector<std::pair<std::string,diskpilot::Category>> sorted(a.extensions.begin(),a.extensions.end());
    std::sort(sorted.begin(),sorted.end(),[](const auto& x,const auto& y){return x.second.logical_bytes>y.second.logical_bytes;});
    for(std::size_t i=0;i<std::min<std::size_t>(24,sorted.size());++i)list<<QVariantMap{{"extension",sorted[i].first.empty()?"(no extension)":QString::fromStdString(sorted[i].first)},{"size",bytes(sorted[i].second.logical_bytes)}};
    return list;
}
void Controller::navigate(int i){if(busy_||!hasScan()||i<0||static_cast<std::size_t>(i)>=scan_->nodes.size())return;if(scan_->nodes[static_cast<std::size_t>(i)].kind==diskpilot::NodeKind::directory){directory_=static_cast<std::size_t>(i);emit stateChanged();}}
void Controller::up(){if(hasScan()&&directory_!=0)navigate(static_cast<int>(scan_->nodes[directory_].parent));}
void Controller::home(){navigate(0);}
void Controller::setFilter(const QString& f,double minimumMiB){filter_=f;minimum_=std::isfinite(minimumMiB)&&minimumMiB>0?static_cast<std::uintmax_t>(std::min(minimumMiB,1.0e9)*1048576.0):0;emit stateChanged();}
QVariantList Controller::tiles(double width,double height)const{
    QVariantList list;if(!hasScan()||width<=0||height<=0)return list;
    const auto view=diskpilot::directory_treemap(*scan_,directory_,{0,0,width,height},1000);
    for(const auto& t:view.tiles){auto r=row(t.node);r["x"]=t.bounds.x;r["y"]=t.bounds.y;r["w"]=t.bounds.width;r["h"]=t.bounds.height;list<<r;}
    return list;
}
void Controller::findDuplicates(){
    if(busy_||!hasScan())return;auto scan=scan_;begin();message("Verifying duplicates…");QPointer<Controller> self(this);
    worker_=std::jthread([self,scan](std::stop_token stop){try{
        auto last=std::chrono::steady_clock::now();
        auto result=diskpilot::DuplicateDetector{}.find(*scan,stop,[&](const auto& p){const auto now=std::chrono::steady_clock::now();if(now-last<std::chrono::milliseconds(100))return;last=now;post(self,[p](auto& c){c.message(QString("Hashed %1 files · read %2").arg(p.files_hashed).arg(bytes(p.bytes_read)));});});
        post(self,[result=std::move(result)](auto& c) mutable {c.duplicates_=std::move(result);c.selectedDuplicates_.clear();c.busy_=false;c.message(c.duplicates_.complete?"Duplicate verification complete.":"Duplicate verification is partial or cancelled. Inspect report errors.");emit c.stateChanged();});
    }catch(const std::exception& e){const auto msg=QString::fromUtf8(e.what());post(self,[msg](auto& c){c.busy_=false;c.message(msg);emit c.stateChanged();});}});
}
QVariantList Controller::duplicateRows()const{
    QVariantList list;if(!hasScan())return list;std::size_t group=0;
    for(const auto& g:duplicates_.groups){++group;for(auto i:g.nodes){if(list.size()>=2000)return list;auto r=row(i);r["group"]=static_cast<int>(group);r["selected"]=selectedDuplicates_.contains(i);r["verification"]=QString::fromStdString(g.verification);list<<r;}}
    return list;
}
void Controller::selectDuplicate(int node,bool selected){
    if(busy_||node<0)return;const auto i=static_cast<std::size_t>(node);
    for(const auto& g:duplicates_.groups)if(std::find(g.nodes.begin(),g.nodes.end(),i)!=g.nodes.end()){
        if(selected){std::size_t chosen=0;for(auto member:g.nodes)chosen+=selectedDuplicates_.contains(member)?1:0;
            if(!selectedDuplicates_.contains(i)&&chosen+1>=g.nodes.size()){message("Keep at least one copy in every group.");emit stateChanged();return;}selectedDuplicates_.insert(i);
        }else selectedDuplicates_.erase(i);emit stateChanged();return;
    }
}
QVariantList Controller::cleanupRows()const{
    QVariantList list;if(!hasScan())return list;
    for(std::size_t i=0;i<std::min<std::size_t>(cleanup_.size(),2000);++i){const auto& c=cleanup_[i];auto r=row(c.node);r["index"]=static_cast<int>(i);r["ecosystem"]=QString::fromStdString(c.ecosystem);r["type"]=QString::fromStdString(c.type);r["evidence"]=QString::fromStdString(c.evidence);r["rebuild"]=QString::fromStdString(c.rebuild);r["warning"]=QString::fromStdString(c.warning);r["protected"]=c.confidence==diskpilot::Confidence::protected_path;r["selected"]=selectedCleanup_.contains(i);list<<r;}
    return list;
}
void Controller::selectCleanup(int index,bool selected){if(busy_||index<0||static_cast<std::size_t>(index)>=cleanup_.size())return;if(selected)selectedCleanup_.insert(static_cast<std::size_t>(index));else selectedCleanup_.erase(static_cast<std::size_t>(index));plan_.clear();emit stateChanged();}
void Controller::previewCleanup(){
    if(busy_||!hasScan())return;const auto scan=scan_;const auto candidates=cleanup_;const std::vector<std::size_t> selected(selectedCleanup_.begin(),selectedCleanup_.end());
    if(selected.empty()){message("Select candidates to review first.");return;}begin();QPointer<Controller> self(this);
    worker_=std::jthread([self,scan,candidates,selected](std::stop_token stop){try{
        if(stop.stop_requested())throw std::runtime_error("Preview cancelled");
        const auto plan=diskpilot::make_cleanup_plan(*scan,candidates,selected,stop);QString result="REVIEW ONLY — no files will be deleted.\n\n";
        for(const auto& item:plan.items)result+=text(item.path)+"\n"+bytes(item.candidate.logical_bytes)+" · "+QString::number(item.manifest.size())+" entries revalidated\n"+QString::fromStdString(item.candidate.rebuild)+"\n\n";
        result+="Selected logical size: "+bytes(plan.logical_bytes)+"\nThis is not a guarantee of reclaimable disk space.\nNative trash execution is disabled pending validation of protection against concurrent path replacement.";
        post(self,[result](auto& c){c.plan_=result;c.busy_=false;c.message("Cleanup review prepared. No files changed.");emit c.stateChanged();});
    }catch(const std::exception& e){const auto msg=QString::fromUtf8(e.what());post(self,[msg](auto& c){c.busy_=false;c.plan_=msg;c.message("Cleanup preview was refused. No files changed.");emit c.stateChanged();});}});
}
void Controller::exportReport(const QUrl& url,const QString& format){
    if(busy_||!hasScan()||!url.isLocalFile())return;auto scan=scan_;const auto duplicates=duplicates_;const auto cleanup=cleanup_;const auto file=path(url.toLocalFile());const auto type=format.toStdString();begin();QPointer<Controller> self(this);
    worker_=std::jthread([self,scan,duplicates,cleanup,file,type](std::stop_token){try{std::ostringstream out;diskpilot::write_report(out,*scan,diskpilot::analyze(*scan),type,&duplicates,&cleanup);diskpilot::write_new_file(file,out.str());post(self,[](auto& c){c.busy_=false;c.message("Report exported.");emit c.stateChanged();});}catch(const std::exception& e){const auto msg=QString::fromUtf8(e.what());post(self,[msg](auto& c){c.busy_=false;c.message(msg);emit c.stateChanged();});}});
}
void Controller::saveSnapshot(const QUrl& url){
    if(busy_||!hasScan()||!url.isLocalFile())return;auto scan=scan_;const auto file=path(url.toLocalFile());begin();QPointer<Controller> self(this);
    worker_=std::jthread([self,scan,file](std::stop_token){try{std::ostringstream out;diskpilot::write_snapshot(out,diskpilot::snapshot(*scan));diskpilot::write_new_file(file,out.str());post(self,[](auto& c){c.busy_=false;c.message("Metadata snapshot saved.");emit c.stateChanged();});}catch(const std::exception& e){const auto msg=QString::fromUtf8(e.what());post(self,[msg](auto& c){c.busy_=false;c.message(msg);emit c.stateChanged();});}});
}
void Controller::compareSnapshot(const QUrl& url){
    if(busy_||!hasScan()||!url.isLocalFile())return;auto scan=scan_;const auto file=path(url.toLocalFile());begin();QPointer<Controller> self(this);
    worker_=std::jthread([self,scan,file](std::stop_token){try{std::ifstream in(file,std::ios::binary);if(!in)throw std::runtime_error("Cannot open snapshot");const auto old=diskpilot::read_snapshot(in),now=diskpilot::snapshot(*scan);const auto changes=diskpilot::compare(old,now);std::ostringstream out;diskpilot::write_comparison(out,old,now,changes);const auto result=QString::fromUtf8(out.str());post(self,[result](auto& c){c.comparison_=result;c.busy_=false;c.message("Snapshot comparison complete.");emit c.stateChanged();});}catch(const std::exception& e){const auto msg=QString::fromUtf8(e.what());post(self,[msg](auto& c){c.busy_=false;c.message(msg);emit c.stateChanged();});}});
}
void Controller::openLocation(int i){if(busy_||!hasScan()||i<0||static_cast<std::size_t>(i)>=scan_->nodes.size())return;const auto p=scan_->nodes[static_cast<std::size_t>(i)].path.parent_path();if(!diskpilot::link_free_path(p)){message("Location changed or contains links.");return;}if(!QDesktopServices::openUrl(QUrl::fromLocalFile(text(p))))message("Could not open location.");}
void Controller::copyPath(int i){if(hasScan()&&i>=0&&static_cast<std::size_t>(i)<scan_->nodes.size())QGuiApplication::clipboard()->setText(text(scan_->nodes[static_cast<std::size_t>(i)].path));}
