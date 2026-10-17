#pragma once
#include "diskpilot/scanner.hpp"
#include <gtest/gtest.h>
#include <atomic>
#include <fstream>

class DiskFixture : public ::testing::Test {
  protected:
    diskpilot::fs::path root;
    void SetUp() override {
        static std::atomic<unsigned> counter{0};
        root=diskpilot::fs::temp_directory_path()/("diskpilot-fixture-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(counter++));
        ASSERT_TRUE(diskpilot::fs::create_directory(root));
    }
    void TearDown() override {
        std::error_code ec;diskpilot::fs::remove_all(root,ec);
    }
    void write(const diskpilot::fs::path& name,const std::string& data) {
        diskpilot::fs::create_directories((root/name).parent_path());
        std::ofstream f(root/name,std::ios::binary);f<<data;f.close();ASSERT_TRUE(f.good());
    }
    diskpilot::ScanResult scan() {return diskpilot::FileSystemScanner{}.scan(root);}
};
