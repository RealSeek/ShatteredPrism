// SPDX-License-Identifier: GPL-3.0-only

#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <cstring>

#include "net/ByteArraySink.h"
#include "net/NetJob.h"
#include "net/NetRequest.h"

class TestReply : public QNetworkReply {
   public:
    TestReply(const QNetworkRequest& request, bool succeed)
    {
        setRequest(request);
        setUrl(request.url());
        open(QIODevice::ReadOnly);
        if (succeed) {
            QTimer::singleShot(0, this, [this] {
                m_data = "mod contents";
                setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
                emit readyRead();
                setFinished(true);
                emit finished();
            });
        }
    }

    void abort() override
    {
        setError(OperationCanceledError, "Operation canceled");
        emit errorOccurred(OperationCanceledError);
        setFinished(true);
        emit finished();
    }

    qint64 bytesAvailable() const override { return m_data.size() + QNetworkReply::bytesAvailable(); }

   protected:
    qint64 readData(char* data, qint64 max_size) override
    {
        if (m_data.isEmpty())
            return -1;
        const auto size = qMin(max_size, qint64(m_data.size()));
        std::memcpy(data, m_data.constData(), size);
        m_data.remove(0, size);
        return size;
    }

   private:
    QByteArray m_data;
};

// The first reply stays queued forever; Qt's own transfer timer never runs.
class QueuedRequest : public Net::NetRequest {
   public:
    QueuedRequest()
    {
        m_url = QUrl("https://edge.forgecdn.net/files/mod.jar");
        m_sink = std::make_unique<Net::ByteArraySink>(output);
    }

    std::shared_ptr<QByteArray> output = std::make_shared<QByteArray>();
    QList<QNetworkRequest> requests;
    bool fallbackSucceeds = true;

   protected:
    QNetworkReply* getReply(QNetworkRequest& request) override
    {
        request.setTransferTimeout(30);
        requests.append(request);
        return new TestReply(request, requests.size() > 1 && fallbackSucceeds);
    }
};

class NetRequestTest : public QObject {
    Q_OBJECT

   private slots:
    void stalledRequestFails()
    {
        auto request = makeShared<QueuedRequest>();
        NetJob job("stalled download", {}, 1);
        job.setAskRetry(false);
        job.setAutoRetryLimit(1);
        job.addNetAction(request);
        QSignalSpy failed(&job, &Task::failed);
        QSignalSpy succeeded(&job, &Task::succeeded);
        job.start();

        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(succeeded.count(), 0);
        QVERIFY(request->failReason().contains(request->url().toString()));
        QVERIFY(request->failReason().contains("timed out"));
        QVERIFY(!request->requests.front().attribute(QNetworkRequest::Http2AllowedAttribute).toBool());
    }

    void fallbackCompletesOneDownload()
    {
        auto request = makeShared<QueuedRequest>();
        const QUrl fallback("https://mirror.example/mod.jar");
        request->setFallbackUrl(fallback);
        NetJob job("fallback download", {}, 1);
        job.addNetAction(request);
        QSignalSpy failed(&job, &Task::failed);
        QSignalSpy succeeded(&job, &Task::succeeded);
        job.start();

        QTRY_COMPARE(succeeded.count(), 1);
        QCOMPARE(failed.count(), 0);
        QCOMPARE(job.size(), 1);
        QCOMPARE(request->requests.size(), 2);
        QCOMPARE(request->requests.back().url(), fallback);
        QCOMPARE(*request->output, QByteArray("mod contents"));
        QVERIFY(request->requests.back().attribute(QNetworkRequest::Http2AllowedAttribute, true).toBool());
    }

    void fallbackFailureIsReported()
    {
        auto request = makeShared<QueuedRequest>();
        request->fallbackSucceeds = false;
        request->setFallbackUrl(QUrl("https://mirror.example/mod.jar"));
        NetJob job("failed fallback", {}, 1);
        job.setAskRetry(false);
        job.setAutoRetryLimit(1);
        job.addNetAction(request);
        QSignalSpy failed(&job, &Task::failed);
        QSignalSpy succeeded(&job, &Task::succeeded);
        job.start();

        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(succeeded.count(), 0);
        QCOMPARE(request->requests.size(), 2);
        QVERIFY(request->failReason().contains("https://mirror.example/mod.jar"));
    }

    void abortDoesNotStartFallback()
    {
        QueuedRequest request;
        request.setFallbackUrl(QUrl("https://mirror.example/mod.jar"));
        QSignalSpy aborted(&request, &Task::aborted);
        QSignalSpy failed(&request, &Task::failed);
        request.start();
        QVERIFY(request.abort());

        QCOMPARE(aborted.count(), 1);
        QCOMPARE(failed.count(), 0);
        QCOMPARE(request.requests.size(), 1);
    }
};

QTEST_GUILESS_MAIN(NetRequestTest)

#include "NetRequest_test.moc"
