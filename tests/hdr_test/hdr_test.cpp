// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <iostream>
#include <fstream>
#include <regex>
#include <string>
#include <chrono>

template <typename T>
int loadFile(char *filename, uint32_t num_element, T* data, bool swap_endianess) {
    if(!filename || !data) {
        throw std::string("Null pointer.");
    }

    FILE *file = fopen(filename, "rb");
    if(!file) {
        throw std::string("Read file error: ").append(filename);
    }

    fread(data, sizeof(T), num_element, file);
    if(swap_endianess) {
		for(size_t i = 0; i < num_element; i++) {
			data[i] = static_cast<T>(_byteswap_ushort(static_cast<uint16_t>(data[i])));
		}
    }

    return 0;
}

void mergeFramesUsingOnlyDepth(uint16_t *new_data, uint16_t *d0, uint16_t *d1, int width, int height) {
    for(int i = 0; i < width * height; i++) {
        if(d0[i] && d1[i]) {
            if(d0[i] == 65535)
                new_data[i] = d1[i];
            else
                new_data[i] = d0[i];
        }
        else if(d0[i])
            new_data[i] = d0[i];
        else if(d1[i])
            new_data[i] = d1[i];
    }
}

template <typename T>
void triangleWeights(float* w) {
    if(!w)
        return;
    int   length = 1 << (sizeof(T) * 8); 
    int   half   = length / 2;
    memset(w, 0, sizeof(float) * length);
    for(int i = 0; i < length; i++) {
        *w++ = i < half ? (1.f * i / half) : (2 - 1.f * i / half);
    }
}

template <typename T>
void censusStd(const T* ir, int width, int height, uint8_t* std, uint8_t ws = 7) {
    int ws_2 = ws / 2;
    int thresh = 10;
    memset(std, 0, sizeof(uint8_t) * width * height);

    for(int v = ws_2; v < height - ws_2; v++) {
        int      row_start_idx = v * width + ws_2;
        const T *p_ir_center   = ir + row_start_idx;
        uint8_t *p_std         = std + row_start_idx;

        for(int u = ws_2; u < width - ws_2; u++) {
			uint8_t  count         = 0;

            const T *p_ir_row = p_ir_center - ws_2 * width - ws_2;
            for(int wv = -ws_2; wv < ws_2; wv+=1) {
                const T *p_ir_col = p_ir_row;
                for(int wu = -ws_2; wu < ws_2; wu+=1) {
                    if((*p_ir_col - *p_ir_center < -thresh) || (*p_ir_col - *p_ir_center > thresh))
                        count++;
					p_ir_col++;
                }
                p_ir_row += width;
            }

            p_ir_center++;
            *p_std++ = count;
        }
    }
}

float EXP_LUT[256];
bool  LUT_Inited = false;
float s_global = 256.f / (7 * 7);

template <typename T> void generateConfidenceMap(const T *ir, uint8_t *map, int width, int height, uint8_t ws = 7) {
    std::cout << ws;

    if (!LUT_Inited) {
		triangleWeights<uint8_t>(EXP_LUT);
    }

    //uint8_t *std_w = (uint8_t *)malloc(width * height * sizeof(uint8_t));
    //memset(std_w, 0, width * height * sizeof(uint8_t));
    //censusStd<uint8_t>(ir, width, height, std_w, ws);

    const uint8_t *p_ir = ir;
    //uint8_t       *p_std = std_w;
    uint8_t *      p_map = map;

    for(int i = 0; i < height; i++) {
        for(int j = 0; j < width; j++) {
            float exp                 = EXP_LUT[*p_ir++];
            //*p_map++ = static_cast<uint8_t>(exp * (*p_std++) * s_global);
            *p_map++ = static_cast<uint8_t>(exp * 255);
        }
    }

    //free(std_w);
}

void mergeFramesUsingIr(uint16_t *new_data, uint16_t *d0, uint16_t *d1, uint8_t* first_ir,
                        uint8_t *second_ir, int width, int height) {
    int      pix_num     = width * height;
    int      map_size    = sizeof(uint8_t) * pix_num;
    uint8_t *confidence0 = (uint8_t *)malloc(map_size);
    uint8_t *confidence1 = (uint8_t *)malloc(map_size);
    if(!confidence0 || !confidence1) {
        return;
    }
    memset(confidence0, 0, sizeof(uint8_t) * pix_num);
    memset(confidence1, 0, sizeof(uint8_t) * pix_num);

	generateConfidenceMap<uint8_t>(first_ir, confidence0, width, height);
	generateConfidenceMap<uint8_t>(second_ir, confidence1, width, height);

    for(int i = 0; i < pix_num; i++) {
        new_data[i] = confidence0[i] > confidence1[i] ? d0[i] : d1[i];
    }
    free(confidence0);
    free(confidence1);
}

template<typename T>
void saveImage(T* data, int element_num, char name[]) {
	FILE *fp = fopen(name, "wb");
	if(fp) {
		fwrite(data, sizeof(T), element_num, fp);
		fclose(fp);
	}
    else {
        throw std::string("Open file error: ").append(name);
    }
}

int main(int argc, char *argv[]) try{
    if(argc < 7) {
        throw std::string("Usage: %s width heght exp0 exp1 depth0 depth1 [ir0] [ir1]");
    }

    int width = std::atoi(argv[1]), height = std::atoi(argv[2]), exp0 = std::atoi(argv[3]), exp1 = std::atoi(argv[4]);
    std::cout << exp0 << exp1 << std::endl;
    int pix_num = width * height;
    int depth_size = pix_num * sizeof(uint16_t);
    int ir_size    = pix_num * sizeof(uint8_t);

    uint16_t *depth0 = (uint16_t *)malloc(depth_size), *depth1 = (uint16_t*)malloc(depth_size);
    uint8_t *ir0 = (uint8_t *)malloc(ir_size), *ir1 = (uint8_t*)malloc(ir_size);
    loadFile<uint16_t>(argv[5], pix_num, depth0, true);
    loadFile<uint16_t>(argv[6], pix_num, depth1, true);
    loadFile<uint8_t>(argv[7], pix_num, ir0, false);
    loadFile<uint8_t>(argv[8], pix_num, ir1, false);

    uint16_t *depth_output = (uint16_t *)malloc(depth_size);
    memset(depth_output, 0, depth_size);

    mergeFramesUsingOnlyDepth(depth_output, depth0, depth1, width, height);

	char nname[256];
	sprintf(nname, "%s_depthonly.raw", argv[5]);
    saveImage<uint16_t>(depth_output, pix_num, nname);

    std::chrono::steady_clock::time_point start, end;
    for(size_t i = 0; i < 100; i++) {
        start = std::chrono::high_resolution_clock::now();
		mergeFramesUsingIr(depth_output, depth0, depth1, ir0, ir1, width, height);
        end = std::chrono::high_resolution_clock::now();
        std::chrono::microseconds d = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << d.count() << std::endl;
    }
	sprintf(nname, "%s_withir.raw", argv[5]);
    saveImage<uint16_t>(depth_output, pix_num, nname);

    free(depth0);
    free(depth1);
    free(ir0);
    free(ir1);
    free(depth_output);

    return 0;
}
catch (const std::string& e)
{
    std::cerr << e << std::endl;
    exit(-1);
}


