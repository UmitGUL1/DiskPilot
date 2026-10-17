#pragma once
#include "diskpilot/reports.hpp"
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QUrl>
#include <set>
#include <thread>

class Controller final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString target READ target WRITE setTarget NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY progressChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool hasScan READ hasScan NOTIFY stateChanged)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap summary READ summary NOTIFY stateChanged)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY stateChanged)
    Q_PROPERTY(QVariantList largest READ largest NOTIFY stateChanged)
    Q_PROPERTY(QVariantList categories READ categories NOTIFY stateChanged)
    Q_PROPERTY(QVariantList duplicateRows READ duplicateRows NOTIFY stateChanged)
    Q_PROPERTY(QVariantList cleanupRows READ cleanupRows NOTIFY stateChanged)
    Q_PROPERTY(QString planPreview READ planPreview NOTIFY stateChanged)
    Q_PROPERTY(QString comparison READ comparison NOTIFY stateChanged)
    Q_PROPERTY(int entryLimit READ entryLimit WRITE setEntryLimit NOTIFY stateChanged)
  public:
    explicit Controller(QObject* parent=nullptr);
    ~Controller() override;
    QString target() const {return target_;}
    void setTarget(const QString&);
    QString status() const {return status_;}
    bool busy() const {return busy_;}
    bool hasScan() const {return scan_&&!scan_->nodes.empty();}
    QString currentPath() const;
    QVariantMap summary() const;
    QVariantList rows() const;
    QVariantList largest() const;
    QVariantList categories() const;
    QVariantList duplicateRows() const;
    QVariantList cleanupRows() const;
    QString planPreview() const {return plan_;}
    QString comparison() const {return comparison_;}
    int entryLimit() const {return entryLimit_;}
    void setEntryLimit(int);
    Q_INVOKABLE void chooseTarget(const QUrl&);
    Q_INVOKABLE void scan();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void navigate(int node);
    Q_INVOKABLE void up();
    Q_INVOKABLE void home();
    Q_INVOKABLE void setFilter(const QString&,double minimumMiB);
    Q_INVOKABLE QVariantList tiles(double width,double height) const;
    Q_INVOKABLE void findDuplicates();
    Q_INVOKABLE void selectDuplicate(int node,bool selected);
    Q_INVOKABLE void selectCleanup(int index,bool selected);
    Q_INVOKABLE void previewCleanup();
    Q_INVOKABLE void exportReport(const QUrl&,const QString& format);
    Q_INVOKABLE void saveSnapshot(const QUrl&);
    Q_INVOKABLE void compareSnapshot(const QUrl&);
    Q_INVOKABLE void openLocation(int node);
    Q_INVOKABLE void copyPath(int node);
  signals:
    void stateChanged();
    void progressChanged();
  private:
    void message(const QString&);
    void begin();
    QVariantMap row(std::size_t) const;
    QString target_,status_="Select a folder to begin.",filter_,plan_,comparison_;
    bool busy_=false;
    int entryLimit_=2'000'000;
    std::uintmax_t minimum_=0;
    std::size_t directory_=0;
    std::shared_ptr<const diskpilot::ScanResult> scan_;
    diskpilot::StorageAnalysis analysis_;
    diskpilot::DuplicateResult duplicates_;
    std::vector<diskpilot::CleanupCandidate> cleanup_;
    std::set<std::size_t> selectedCleanup_,selectedDuplicates_;
    std::jthread worker_;
};
