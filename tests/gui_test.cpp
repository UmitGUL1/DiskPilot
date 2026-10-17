#include "../src/gui/controller.hpp"
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

class GuiTest final : public QObject {
    Q_OBJECT
    static void write(const QString& path, const QByteArray& bytes) {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QCOMPARE(f.write(bytes), bytes.size());
    }
  private slots:
    void scanNavigateVerifyAndExport() {
        QTemporaryDir fixture;
        QTemporaryDir outputs;
        QVERIFY(fixture.isValid());
        QVERIFY(outputs.isValid());
        QVERIFY(QDir(fixture.path()).mkpath("project/__pycache__"));
        write(fixture.filePath("a.dat"), "same");
        write(fixture.filePath("b.dat"), "same");
        write(fixture.filePath("unique.dat"), "else");
        write(fixture.filePath("project/__pycache__/module.pyc"), "bytecode");
        Controller controller;
        controller.setTarget(fixture.path());
        controller.scan();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 20000);
        QVERIFY(controller.hasScan());
        QVERIFY(controller.summary().value("complete").toBool());
        QCOMPARE(controller.summary().value("files").toULongLong(), 4ULL);
        QCOMPARE(controller.rows().size(), 4);
        const auto rootPath = controller.currentPath();
        int project = -1;
        for (const auto& value : controller.rows()) {
            const auto row = value.toMap();
            if (row.value("name") == "project") project = row.value("node").toInt();
        }
        QVERIFY(project >= 0);
        controller.navigate(project);
        QCOMPARE(controller.rows().size(), 1);
        controller.up();
        QCOMPARE(controller.currentPath(), rootPath);
        const auto tiles = controller.tiles(600, 400);
        QVERIFY(!tiles.isEmpty());
        controller.setFilter("unique", 0);
        QCOMPARE(controller.largest().size(), 1);
        controller.setFilter("", 0);
        controller.findDuplicates();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 20000);
        QVERIFY(controller.summary().value("duplicateComplete").toBool());
        QCOMPARE(controller.duplicateRows().size(), 2);
        const int first = controller.duplicateRows()[0].toMap().value("node").toInt();
        const int second = controller.duplicateRows()[1].toMap().value("node").toInt();
        controller.selectDuplicate(first, true);
        controller.selectDuplicate(second, true);
        unsigned selected = 0;
        for (const auto& value : controller.duplicateRows()) selected += value.toMap().value("selected").toBool();
        QCOMPARE(selected, 1U); // One copy must remain unselected.
        QCOMPARE(controller.cleanupRows().size(), 1);
        controller.selectCleanup(0, true);
        controller.previewCleanup();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 20000);
        QVERIFY(controller.planPreview().contains("REVIEW ONLY"));
        QVERIFY(QFile::exists(fixture.filePath("project/__pycache__/module.pyc")));
        const auto snapshot = QUrl::fromLocalFile(outputs.filePath("before.json"));
        controller.saveSnapshot(snapshot);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 20000);
        QVERIFY(QFile::exists(snapshot.toLocalFile()));
        controller.compareSnapshot(snapshot);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 20000);
        QVERIFY(controller.comparison().contains("diskpilot.comparison"));
        const auto report = QUrl::fromLocalFile(outputs.filePath("report.html"));
        controller.exportReport(report, "html");
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 20000);
        QFile f(report.toLocalFile());
        QVERIFY(f.open(QIODevice::ReadOnly));
        QVERIFY(f.readAll().contains("Content-Security-Policy"));
    }
    void invalidRootAndCancellation() {
        QTemporaryDir fixture;
        QVERIFY(fixture.isValid());
        Controller controller;
        controller.setTarget(fixture.filePath("missing"));
        controller.scan();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 20000);
        QVERIFY(!controller.hasScan());
        controller.setTarget(fixture.path());
        controller.scan();
        controller.cancel();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 20000);
        // Cancellation is best effort: an empty scan may finish before the stop
        // request. Either completion path must settle without a stuck worker.
        QVERIFY(controller.hasScan());
        QVERIFY(controller.status().contains("cancelled") || controller.status().contains("complete"));
    }
};
QTEST_MAIN(GuiTest)
#include "gui_test.moc"
