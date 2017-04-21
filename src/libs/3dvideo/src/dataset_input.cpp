#include <opencv2/imgproc.hpp>

#include <util/tiny_logger.hpp>

#include <3dvideo/dataset_input.hpp>


DatasetInput::DatasetInput(const std::string &path)
    : in(path, std::ios::binary)
{
    // metadata parsers
    auto &mp = metadataParsers;
    mp[Field::VERSION] = [&] { return binRead(meta.formatVersion); };
    mp[Field::FRAME_SECTION] = [&] { return true; };
    mp[Field::COLOR_RESOLUTION] = [&] { return binRead(meta.color.w, meta.color.h); };
    mp[Field::DEPTH_RESOLUTION] = [&] { return binRead(meta.depth.w, meta.depth.h); };
    mp[Field::COLOR_INTRINSICS] = [&] { return binRead(meta.color.f, meta.color.cx, meta.color.cy); };
    mp[Field::DEPTH_INTRINSICS] = [&] { return binRead(meta.depth.f, meta.depth.cx, meta.depth.cy); };
    mp[Field::COLOR_FORMAT] = [&] { return binRead(meta.colorFormat); };
    mp[Field::DEPTH_FORMAT] = [&] { return binRead(meta.depthFormat); };

    // color data readers
    auto &cr = colorReaders;
    cr[ColorDataFormat::BGR] = [&](Frame &f)
    {
        f.color = cv::Mat(meta.color.h, meta.color.w, CV_8UC3);
        return bool(in.read((char *)f.color.data, f.color.total()));
    };
    cr[ColorDataFormat::YUV_NV21] = [&](Frame &f)
    {
        // WARNING! Not thread safe (but helps reduce dynamic memory allocation).
        static auto yuvImg = cv::Mat(3 * meta.color.h / 2, meta.color.w, CV_8UC1);
        const bool ok = bool(in.read((char *)yuvImg.data, yuvImg.total()));
        // Rather inefficient to convert each frame, but it's easier to work with. Will optimize if required.
        cv::cvtColor(yuvImg, f.color, cv::COLOR_YUV2BGR_NV21);
        return ok;
    };

    // frame parsers
    auto &fp = frameParsers;
    fp[Field::FRAME_NUMBER] = [&](Frame &f) { return binRead(f.frameNumber); };
    fp[Field::FRAME_SECTION] = [&](Frame &) { return true; };
    fp[Field::COLOR_TIMESTAMP] = [&](Frame &f) { return binRead(f.cTimestamp); };
    fp[Field::DEPTH_TIMESTAMP] = [&](Frame &f) { return binRead(f.dTimestamp); };
    fp[Field::COLOR] = [&](Frame &f) { return cr[meta.colorFormat](f); };
    fp[Field::DEPTH] = [&](Frame &f)
    {
        f.depth = cv::Mat(meta.depth.h, meta.depth.w, CV_16UC1);
        return bool(in.read((char *)f.depth.data, f.depth.total()));
    };
}

Status DatasetInput::readHeader()
{
    if (!in.is_open())
        return Status::ERROR;

    Field field;
    bool ok = binRead(field);
    if (!ok || field != Field::MAGIC)
    {
        TLOG(ERROR) << "Invalid header, could not start reading header!";
        return Status::ERROR;
    }

    ok = binRead(field);
    if (!ok || field != Field::METADATA_SECTION)
    {
        TLOG(ERROR) << "Could not find metadata section";
        return Status::ERROR;
    }

    while (ok && field != Field::FRAME_SECTION)
        ok = readMetadataField(field);

    if (!ok)
    {
        TLOG(ERROR) << "Could not read field " << int(field);
        return Status::ERROR;
    }

    TLOG(INFO) << "Format version is: " << meta.formatVersion;

    return Status::SUCCESS;
}

bool DatasetInput::readMetadataField(Field &field)
{
    const auto ok = binRead(field);
    if (!ok)
    {
        TLOG(ERROR) << "Could not find the next metadata field";
        return false;
    }

    const auto it = metadataParsers.find(field);
    if (it == metadataParsers.end())
    {
        TLOG(ERROR) << "Could not find parser for field " << int(field);
        return false;
    }

    const auto &parser = it->second;
    return parser();
}

Status DatasetInput::readFrame(Frame &frame)
{
    Field field;
    bool ok;
    do {
        ok = readFrameField(frame, field);
    } while (ok && field != Field::FRAME_SECTION);

    if (in.eof())
    {
        TLOG(INFO) << "Finished reading dataset!";
        isFinished = true;
        return Status::SUCCESS;
    }

    if (!ok)
    {
        TLOG(ERROR) << "Error while reading frame!";
        isFinished = true;
        return Status::ERROR;
    }

    return Status::SUCCESS;
}

bool DatasetInput::readFrameField(Frame &frame, Field &field)
{
    const auto ok = binRead(field);
    if (!ok)
    {
        TLOG(WARNING) << "Could not find the next frame field";
        return false;
    }

    const auto it = frameParsers.find(field);
    if (it == frameParsers.end())
    {
        TLOG(ERROR) << "Could not find parser for frame field " << int(field);
        return false;
    }

    const auto &parser = it->second;
    return parser(frame);
}

DatasetMetadata DatasetInput::getMetadata() const
{
    return meta;
}

bool DatasetInput::finished() const
{
    return isFinished;
}

