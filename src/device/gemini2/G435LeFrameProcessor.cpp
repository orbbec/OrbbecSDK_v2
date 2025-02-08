// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G435LeFrameProcessor.hpp"
#include "frame/FrameFactory.hpp"

namespace libobsensor {

int findSequence(const uint8_t *data, uint32_t size) {
    const uint8_t target[]     = { 0xFF, 0xDA, 0x00, 0x0C };
    const size_t  targetLength = sizeof(target) / sizeof(target[0]);

    for(int i = 0; i <= size - targetLength; ++i) {
        bool match = true;
        for(size_t j = 0; j < targetLength; ++j) {
            if(data[i + j] != target[j]) {
                match = false;
                break;
            }
        }
        if(match) {
            return i;  // 返回匹配的起始位置
        }
    }
    return -1;  // 如果没有找到匹配的序列，返回-1
}
// Depth frame processor
G435LeDepthFrameProcessor::G435LeDepthFrameProcessor(IDevice *owner, std::shared_ptr<FrameProcessorContext> context) : DepthFrameProcessor(owner, context) {}

G435LeDepthFrameProcessor::~G435LeDepthFrameProcessor() noexcept {}

std::shared_ptr<Frame> G435LeDepthFrameProcessor::process(std::shared_ptr<const Frame> frame) {
    frame->getData();
    return DepthFrameProcessor::process(frame);
}

// Color frame processor
G435LeColorFrameProcessor::G435LeColorFrameProcessor(IDevice *owner, std::shared_ptr<FrameProcessorContext> context)
    : FrameProcessor(owner, context, OB_SENSOR_COLOR) {}

G435LeColorFrameProcessor::~G435LeColorFrameProcessor() noexcept {}

std::shared_ptr<Frame> G435LeColorFrameProcessor::process(std::shared_ptr<const Frame> frame) {
    auto outFrame = FrameFactory::createFrameFromOtherFrame(frame);

    int index = findSequence(frame->getData(), static_cast<uint32_t>(frame->getDataSize()));
    if (index == -1) {
        LOG_ERROR("findSequence not found");
        return std::shared_ptr<Frame>(); // 返回无效指针
    }

    // 确保索引不会越界
    if (index + 14 >= frame->getDataSize() || index + 14 + 122 >= frame->getDataSize()) {
        LOG_ERROR("Index out of bounds");
        return std::shared_ptr<Frame>(); // 返回无效指针
    }

    std::vector<uint8_t> data;
    data.assign(frame->getData(), frame->getData() + index + 14);
    data.insert(data.end(), frame->getData() + index + 14 + 122, frame->getData() + frame->getDataSize());

    outFrame->updateData(data.data(), data.size());
    outFrame->updateMetadata(frame->getData() + index + 14 + 26, 122 - 26);

    std::ofstream outputFileOrg("frame_data_org.bin", std::ios::binary);  // 创建一个二进制输出文件流对象
    if(outputFileOrg.is_open()) {
        outputFileOrg.write(reinterpret_cast<const char *>(frame->getData()), frame->getDataSize());  // 将frame->getData()写入二进制文件
        outputFileOrg.close();                                                                        // 关闭文件流
    }

    std::ofstream outputFile("frame_data.bin", std::ios::binary);  // 创建一个二进制输出文件流对象
    if(outputFile.is_open()) {
        outputFile.write(reinterpret_cast<const char *>(outFrame->getData()), outFrame->getDataSize());  // 将frame->getData()写入二进制文件
        outputFile.close();                                                                        // 关闭文件流
    }

    std::ofstream outputFileMetadata("frame_data_metadata.bin", std::ios::binary);  // 创建一个二进制输出文件流对象
    if(outputFileMetadata.is_open()) {
        outputFileMetadata.write(reinterpret_cast<const char *>(outFrame->getMetadata()), outFrame->getMetadataSize());  // 将frame->getData()写入二进制文件
        outputFileMetadata.close();                                                                        // 关闭文件流
    }
    return FrameProcessor::process(outFrame);
}

}  // namespace libobsensor
