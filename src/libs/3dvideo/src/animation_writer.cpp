#include <iomanip>

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <util/io_3d.hpp>
#include <util/tiny_logger.hpp>
#include <util/filesystem_utils.hpp>

#include <3dvideo/animation_writer.hpp>


AnimationWriter::AnimationWriter(const std::string &outputPath, MeshFrameQueue &q, CancellationToken &cancellationToken)
    : MeshFrameConsumer(q, cancellationToken)
    , outputPath(outputPath)
    , timeframe(pathJoin(outputPath, "sketchfab.timeframe"), std::ios::out)
{
}

AnimationWriter::~AnimationWriter()
{
    TLOG(INFO);

    timeframe << 0.033f << " " << lastMeshFilename << '\n';
    timeframe.close();
}

void AnimationWriter::process(std::shared_ptr<MeshFrame> &frame)
{
    if (finished)
        return;

    if (!frame->indexedMode || frame->frame2D->frameNumber < lastWrittenFrame)
    {
        timeframe.close();
        finished = true;
        return;
    }

    TLOG(INFO) << frame->frame2D->frameNumber;

    std::ostringstream filenamePrefix;
    filenamePrefix << std::setw(5) << std::setfill('0') << frame->frame2D->frameNumber << "_";
    const std::string textureFilename = filenamePrefix.str() + "texture.jpg", meshFilename = filenamePrefix.str() + "mesh.ply";

    std::vector<cv::Point3f> points(frame->cloud);
    for (size_t i = 0; i < points.size(); ++i)
        points[i].x *= -1, points[i].y *= -1;

    std::vector<cv::Point2f> uv(frame->uv.size());
    for (size_t i = 0; i < uv.size(); ++i)
        uv[i].x = frame->uv[i].x, uv[i].y = 1.0f - frame->uv[i].y;

    saveBinaryPly(pathJoin(outputPath, meshFilename), &points, &frame->triangles, &uv, &textureFilename);

    cv::Mat finalTexture;
    cv::resize(frame->frame2D->color, finalTexture, cv::Size(), 0.5, 0.5);
    cv::imwrite(pathJoin(outputPath, textureFilename), finalTexture);

    if (lastWrittenFrame != -1)
    {
        const auto timeDeltaSeconds = float(frame->frame2D->dTimestamp - lastFrameTimestamp) / 1000000;
        timeframe << timeDeltaSeconds << " " << lastMeshFilename << '\n';
    }

    lastFrameTimestamp = frame->frame2D->dTimestamp;
    lastWrittenFrame = frame->frame2D->frameNumber;
    lastMeshFilename = meshFilename;
}
