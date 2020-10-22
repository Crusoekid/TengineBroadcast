#define clip(x, y) (x < 0 ? 0 : (x > y ? y : x))

#include "UltraFace.hpp"

using namespace std;

UltraFace::UltraFace(const std::string &tengine_path,
                     int input_width, int input_length, int num_thread_,
                     float score_threshold_, float iou_threshold_, int topk_) {
    num_thread = num_thread_;
    score_threshold = score_threshold_;
    iou_threshold = iou_threshold_;
    in_w = input_width;
    in_h = input_length;
    w_h_list = {in_w, in_h};

    for (auto size : w_h_list) {
        std::vector<float> fm_item;
        for (float stride : strides) {
            fm_item.push_back(ceil(size / stride));
        }
        featuremap_size.push_back(fm_item);
    }

    for (auto size : w_h_list) {
        shrinkage_size.push_back(strides);
    }
    /* generate prior anchors */
    for (int index = 0; index < num_featuremap; index++) {
        float scale_w = in_w / shrinkage_size[0][index];
        float scale_h = in_h / shrinkage_size[1][index];
        for (int j = 0; j < featuremap_size[1][index]; j++) {
            for (int i = 0; i < featuremap_size[0][index]; i++) {
                float x_center = (i + 0.5) / scale_w;
                float y_center = (j + 0.5) / scale_h;

                for (float k : min_boxes[index]) {
                    float w = k / in_w;
                    float h = k / in_h;
                    priors.push_back({clip(x_center, 1), clip(y_center, 1), clip(w, 1), clip(h, 1)});
                }
            }
        }
    }
    /* generate prior anchors finished */

    num_anchors = priors.size();

    if (init_tengine() != 0)
    {
        fprintf(stderr, "Initial tengine failed.\n");
        exit(0);
    }

    cout<<tengine_path<<endl;
    graph = create_graph(nullptr, "tengine", tengine_path.c_str());


    int dims[] = {1, 3, 240, 320};

    input_tensor = get_graph_tensor(graph, "input");


    if (nullptr == input_tensor)
    {
        printf("Get input tensor failed\n");
        exit(0);
    }

    if (0 != set_tensor_shape(input_tensor, dims, 4))
    {
        printf("Set input tensor shape failed\n");
        exit(0);
    }

    if (0 != prerun_graph(graph))
    {
        printf("Pre-run graph failed\n");
        exit(0);
    }

}

UltraFace::~UltraFace() {
    release_graph_tensor(input_tensor);
    postrun_graph(graph);
    destroy_graph(graph);
    release_tengine();
}

int UltraFace::detect(cv::Mat &raw_image, std::vector<FaceInfo> &face_list) {
    if (raw_image.empty()) {
        std::cout << "image is empty ,please check!" << std::endl;
        return -1;
    }

    image_h = raw_image.rows;
    image_w = raw_image.cols;

    int img_size      = in_w * in_h * 3;
    float* input_data = ( float* )malloc(img_size * sizeof(float));
    get_input_data_cv(raw_image, input_data, in_w, in_h, mean_vals, norm_vals, 0);

    if (set_tensor_buffer(input_tensor, input_data, (in_w * in_h * 3) * 4) < 0)
    {
        printf("Set input tensor buffer failed\n");
        return -1;
    }

    auto start = chrono::steady_clock::now();


    // run network
    if (run_graph(graph, 1) < 0)
    {
        printf("Run graph failed\n");
        return -1;
    }

    // get output data

    string scores = "scores";
    string boxes = "boxes";
    tensor_t tensor_scores = get_graph_tensor(graph, scores.c_str());
    tensor_t tensor_boxes = get_graph_tensor(graph, boxes.c_str());

    std::vector<FaceInfo> bbox_collection;


    auto end = chrono::steady_clock::now();
    chrono::duration<double> elapsed = end - start;
    cout << "inference time:" << elapsed.count() << " s" << endl;

    generateBBox(bbox_collection, tensor_scores, tensor_boxes);
    nms(bbox_collection, face_list);

    free(input_data);

    return 0;
}

void UltraFace::generateBBox(std::vector<FaceInfo> &bbox_collection, tensor_t scores, tensor_t boxes) {
    float* scores_blob = ( float* )get_tensor_buffer(scores);
    float* boxes_blob = ( float* )get_tensor_buffer(boxes);
    for (int i = 0; i < num_anchors; i++) {
        if (scores_blob[i * 2 + 1] > score_threshold) {
            FaceInfo rects;
            float x_center = boxes_blob[i * 4] * center_variance * priors[i][2] + priors[i][0];
            float y_center = boxes_blob[i * 4 + 1] * center_variance * priors[i][3] + priors[i][1];
            float w = exp(boxes_blob[i * 4 + 2] * size_variance) * priors[i][2];
            float h = exp(boxes_blob[i * 4 + 3] * size_variance) * priors[i][3];

            rects.x1 = clip(x_center - w / 2.0, 1) * image_w;
            rects.y1 = clip(y_center - h / 2.0, 1) * image_h;
            rects.x2 = clip(x_center + w / 2.0, 1) * image_w;
            rects.y2 = clip(y_center + h / 2.0, 1) * image_h;
            rects.score = clip(scores_blob[i * 2 + 1], 1);
            bbox_collection.push_back(rects);
        }
    }
}

void UltraFace::nms(std::vector<FaceInfo> &input, std::vector<FaceInfo> &output, int type) {
    std::sort(input.begin(), input.end(), [](const FaceInfo &a, const FaceInfo &b) { return a.score > b.score; });

    int box_num = input.size();

    std::vector<int> merged(box_num, 0);

    for (int i = 0; i < box_num; i++) {
        if (merged[i])
            continue;
        std::vector<FaceInfo> buf;

        buf.push_back(input[i]);
        merged[i] = 1;

        float h0 = input[i].y2 - input[i].y1 + 1;
        float w0 = input[i].x2 - input[i].x1 + 1;

        float area0 = h0 * w0;

        for (int j = i + 1; j < box_num; j++) {
            if (merged[j])
                continue;

            float inner_x0 = input[i].x1 > input[j].x1 ? input[i].x1 : input[j].x1;
            float inner_y0 = input[i].y1 > input[j].y1 ? input[i].y1 : input[j].y1;

            float inner_x1 = input[i].x2 < input[j].x2 ? input[i].x2 : input[j].x2;
            float inner_y1 = input[i].y2 < input[j].y2 ? input[i].y2 : input[j].y2;

            float inner_h = inner_y1 - inner_y0 + 1;
            float inner_w = inner_x1 - inner_x0 + 1;

            if (inner_h <= 0 || inner_w <= 0)
                continue;

            float inner_area = inner_h * inner_w;

            float h1 = input[j].y2 - input[j].y1 + 1;
            float w1 = input[j].x2 - input[j].x1 + 1;

            float area1 = h1 * w1;

            float score;

            score = inner_area / (area0 + area1 - inner_area);

            if (score > iou_threshold) {
                merged[j] = 1;
                buf.push_back(input[j]);
            }
        }
        switch (type) {
            case hard_nms: {
                output.push_back(buf[0]);
                break;
            }
            case blending_nms: {
                float total = 0;
                for (int i = 0; i < buf.size(); i++) {
                    total += exp(buf[i].score);
                }
                FaceInfo rects;
                memset(&rects, 0, sizeof(rects));
                for (int i = 0; i < buf.size(); i++) {
                    float rate = exp(buf[i].score) / total;
                    rects.x1 += buf[i].x1 * rate;
                    rects.y1 += buf[i].y1 * rate;
                    rects.x2 += buf[i].x2 * rate;
                    rects.y2 += buf[i].y2 * rate;
                    rects.score += buf[i].score * rate;
                }
                output.push_back(rects);
                break;
            }
            default: {
                printf("wrong type of nms.");
                exit(-1);
            }
        }
    }
}

void UltraFace::get_input_data_cv(const cv::Mat& sample, float* input_data, int img_w, int img_h, const float* mean, const float* scale, int swapRB)
{
    cv::Mat img;
    if(sample.channels() == 4)
    {
        cv::cvtColor(sample, img, cv::COLOR_BGRA2BGR);
    }
    else if(sample.channels() == 1)
    {
        cv::cvtColor(sample, img, cv::COLOR_GRAY2BGR);
    }
    else if(sample.channels() == 3 && swapRB == 1)
    {
        cv::cvtColor(sample, img, cv::COLOR_BGR2RGB);
    }
    else
    {
        img = sample;
    }

    cv::resize(img, img, cv::Size(img_w, img_h));
    img.convertTo(img, CV_32FC3);
    float* img_data = ( float* )img.data;
    int hw = img_w * img_h;
    for(int w = 0; w < img_w; w++)
    {
        for(int h = 0; h < img_h; h++)
        {
            for(int c = 0; c < 3; c++)
            {
                input_data[c * hw + w * img_h + h] = (*img_data - mean[c]) * scale[c];
                img_data++;
            }
        }
    }
}