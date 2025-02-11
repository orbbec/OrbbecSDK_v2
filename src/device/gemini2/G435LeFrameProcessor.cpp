// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "G435LeFrameProcessor.hpp"
#include "frame/FrameFactory.hpp"

namespace libobsensor {
struct G435LeMetadataHearder {
    uint8_t  magic[8];  // fixed magic:"ORBBEC"
    uint32_t data_len;  // Length of the entire metadata data structure

    bool validateMagic() {
        const char expectedMagic[8] = "ORBBEC";
        return std::memcmp(magic, expectedMagic, sizeof(expectedMagic)) == 0;
    }
};

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
    return FrameProcessor::process(frame);
    // auto outFrame = FrameFactory::createFrameFromOtherFrame(frame);

    // int index = findSequence(frame->getData(), static_cast<uint32_t>(frame->getDataSize()));
    // if (index == -1) {
    //     return FrameProcessor::process(frame);
    // }
    
    // const int fixedDistanse = 14; 
    
    // if (index + 14 >= frame->getDataSize()) {
    //     return FrameProcessor::process(frame);
    // }    

    // G435LeMetadataHearder *header = reinterpret_cast<G435LeMetadataHearder *>(const_cast<uint8_t *>(frame->getData() + index + fixedDistanse));

    // if(!header->validateMagic()) {
    //     return FrameProcessor::process(frame);
    // }

    // const int metadatasize = header->data_len;

    // if (index + fixedDistanse + metadatasize >= frame->getDataSize()) {
    //     LOG_ERROR("Index out of bounds");
    //     return FrameProcessor::process(frame);
    // }

    // std::vector<uint8_t> data;
    // data.assign(frame->getData(), frame->getData() + index + fixedDistanse - 4);
    // data.insert(data.end(), frame->getData() + index + fixedDistanse + metadatasize, frame->getData() + frame->getDataSize());

    // outFrame->updateData(data.data(), data.size());
    // outFrame->updateMetadata(frame->getData() + index + fixedDistanse, metadatasize);

    // std::ofstream outputFileOrg("frame_data_org.bin", std::ios::binary); 
    // if(outputFileOrg.is_open()) {
    //     outputFileOrg.write(reinterpret_cast<const char *>(frame->getData()), frame->getDataSize());  
    //     outputFileOrg.close();                                                                       
    // }

    // std::ofstream outputFile("frame_data.bin", std::ios::binary);  
    // if(outputFile.is_open()) {
    //     outputFile.write(reinterpret_cast<const char *>(outFrame->getData()), outFrame->getDataSize());  
    //     outputFile.close();                                                                        
    // }

    // std::ofstream outputFileMetadata("frame_data_metadata.bin", std::ios::binary);  
    // if(outputFileMetadata.is_open()) {
    //     outputFileMetadata.write(reinterpret_cast<const char *>(outFrame->getMetadata()), outFrame->getMetadataSize());  
    //     outputFileMetadata.close();                                                                        
    // }
    //return FrameProcessor::process(outFrame);
}
int G435LeColorFrameProcessor::findSequence(const uint8_t *data, uint32_t size) {
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
            return i; 
        }
    }
    return -1;  
}

}  // namespace libobsensor
