/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <QAbstractVideoBuffer>
#include <QGuiApplication>
#include "videoframe.hpp"

#define CHECK(condition) do { if (!(condition)) qFatal("line %d: %s", __LINE__, #condition); } while (false)

struct Counts { int maps = 0; int unmaps = 0; int destroyed = 0; };
class Buffer : public QAbstractVideoBuffer
{
    Counts& counts;
    bool readable;
    QByteArray pixels;
public:
    Buffer(Counts& counts, bool readable) : counts(counts), readable(readable), pixels(8 * 4 * 4, '\0')
    {
        for (int i = 0; i < pixels.size(); i++)
            pixels[i] = static_cast<char>(i * 17);
    }
    ~Buffer() override { ++counts.destroyed; }
    QVideoFrameFormat format() const override { return {{8, 4}, QVideoFrameFormat::Format_RGBA8888}; }
    MapData map(QVideoFrame::MapMode mode) override
    {
        ++counts.maps;
        CHECK(mode == QVideoFrame::ReadOnly);
        if (!readable)
            return {};
        MapData result;
        result.planeCount = 1;
        result.bytesPerLine[0] = 8 * 4;
        result.dataSize[0] = pixels.size();
        result.data[0] = reinterpret_cast<uchar*>(pixels.data());
        return result;
    }
    void unmap() override { ++counts.unmaps; }
};

static QByteArray serialize(const VideoFrame& frame)
{
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream << frame;
    CHECK(stream.status() == QDataStream::Ok);
    return bytes;
}

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    Counts counts;
    {
        QVideoFrame source(std::make_unique<Buffer>(counts, true));
        source.setStartTime(123456);
        VideoFrame deferred;
        deferred.update(Input_Left_Right_Half, Surround_Off, source, true, true);
        CHECK(counts.maps == 0);
        CHECK(deferred.storage == VideoFrame::Storage_Deferred);
        CHECK(deferred.width == 8 && deferred.height == 4);
        deferred.reUpdate();
        CHECK(counts.maps == 0);
        const auto transported = serialize(deferred);
        CHECK(counts.maps == 1 && counts.unmaps == 1);
        CHECK(!source.isMapped());
        CHECK(deferred.storage == VideoFrame::Storage_Deferred);
        VideoFrame mapped;
        mapped.update(Input_Left_Right_Half, Surround_Off, source, true);
        CHECK(counts.maps == 2);
        CHECK(transported == serialize(mapped));
        VideoFrame received;
        QDataStream reader(transported);
        reader >> received;
        CHECK(reader.status() == QDataStream::Ok);
        CHECK(received.storage == VideoFrame::Storage_Copied);
        CHECK(received.planeCount == 1 && received.bytesPerPlane[0] == 128);
        CHECK(QByteArray(reinterpret_cast<const char*>(received.bits[0].data()), received.bits[0].size()) == QByteArray(reinterpret_cast<const char*>(mapped.mappedBits[0]), 128));
        CHECK(mapped.mapPixels() && counts.maps == 2);
        mapped.invalidate();
        CHECK(counts.unmaps == 2);
        CHECK(deferred.mapPixels() && counts.maps == 3);
        CHECK(deferred.mapPixels() && counts.maps == 3);
        deferred.invalidate();
        CHECK(counts.unmaps == 3);
    }
    CHECK(counts.destroyed == 1);
    Counts failed;
    {
        QVideoFrame source(std::make_unique<Buffer>(failed, false));
        VideoFrame frame;
        frame.update(Input_Left_Right_Half, Surround_Off, source, true, true);
        CHECK(failed.maps == 0);
        CHECK(!frame.mapPixels() && failed.maps == 1);
        CHECK(frame.storage == VideoFrame::Storage_Deferred && frame.planeCount == 0);
        QByteArray bytes;
        QDataStream writer(&bytes, QIODevice::WriteOnly);
        writer << frame;
        CHECK(writer.status() == QDataStream::WriteFailed && bytes.isEmpty());
        CHECK(failed.maps == 2 && failed.unmaps == 0);
    }
    CHECK(failed.destroyed == 1);
    qInfo("PASS: deferred metadata, re-update, one-time mapping, unchanged transport bytes, failed mapping, leases");
}
