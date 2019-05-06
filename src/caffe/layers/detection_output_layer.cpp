#include <algorithm>
#include <fstream>  // NOLINT(readability/streams)
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "boost/filesystem.hpp"
#include "boost/foreach.hpp"

#include "caffe/layers/detection_output_layer.hpp"



namespace caffe {

template <typename Dtype>
void DetectionOutputLayer<Dtype>::LayerSetUp(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
  const DetectionOutputParameter& detection_output_param =
      this->layer_param_.detection_output_param();
  CHECK(detection_output_param.has_num_classes()) << "Must specify num_classes";
  CHECK(detection_output_param.has_attri_type()) << "Must specify attri_type";
  attri_type_ = detection_output_param.attri_type();
  if(attri_type_ == DetectionOutputParameter_AnnoataionAttriType_FACE){
    num_blur_ = detection_output_param.num_blur();
    num_occlusion_ = detection_output_param.num_occlusion();
    blur_permute_.ReshapeLike(*(bottom[3]));
    occlu_permute_.ReshapeLike(*(bottom[4]));
  }else if(attri_type_ == DetectionOutputParameter_AnnoataionAttriType_LPnumber){
    num_chinese_ = detection_output_param.num_lpchinese();
    num_english_ = detection_output_param.num_lpenglish();
    num_letter_ = detection_output_param.num_lpletter();
    chinese_permute_.ReshapeLike(*(bottom[3]));
    english_permute_.ReshapeLike(*(bottom[4]));
    letter_1_permute_.ReshapeLike(*(bottom[5]));
    letter_2_permute_.ReshapeLike(*(bottom[6]));
    letter_3_permute_.ReshapeLike(*(bottom[7]));
    letter_4_permute_.ReshapeLike(*(bottom[8]));
    letter_5_permute_.ReshapeLike(*(bottom[9]));
  }
  num_classes_ = detection_output_param.num_classes();
  share_location_ = detection_output_param.share_location();
  num_loc_classes_ = share_location_ ? 1 : num_classes_;
  background_label_id_ = detection_output_param.background_label_id();
  code_type_ = detection_output_param.code_type();
  variance_encoded_in_target_ =
      detection_output_param.variance_encoded_in_target();
  keep_top_k_ = detection_output_param.keep_top_k();
  confidence_threshold_ = detection_output_param.has_confidence_threshold() ?
      detection_output_param.confidence_threshold() : -FLT_MAX;
  // Parameters used in nms.
  nms_threshold_ = detection_output_param.nms_param().nms_threshold();
  CHECK_GE(nms_threshold_, 0.) << "nms_threshold must be non negative.";
  eta_ = detection_output_param.nms_param().eta();
  CHECK_GT(eta_, 0.);
  CHECK_LE(eta_, 1.);
  top_k_ = -1;
  if (detection_output_param.nms_param().has_top_k()) {
    top_k_ = detection_output_param.nms_param().top_k();
  }
  const SaveOutputParameter& save_output_param =
      detection_output_param.save_output_param();
  output_directory_ = save_output_param.output_directory();
  if (!output_directory_.empty()) {
    if (boost::filesystem::is_directory(output_directory_)) {
      // boost::filesystem::remove_all(output_directory_);
    }
    if (!boost::filesystem::create_directories(output_directory_)) {
        LOG(WARNING) << "Failed to create directory: " << output_directory_;
    }
  }
  output_name_prefix_ = save_output_param.output_name_prefix();
  need_save_ = output_directory_ == "" ? false : true;
  output_format_ = save_output_param.output_format();
  if (save_output_param.has_label_map_file()) {
    string label_map_file = save_output_param.label_map_file();
    if (label_map_file.empty()) {
      // Ignore saving if there is no label_map_file provided.
      LOG(WARNING) << "Provide label_map_file if output results to files.";
      need_save_ = false;
    } else {
      LabelMap label_map;
      CHECK(ReadProtoFromTextFile(label_map_file, &label_map))
          << "Failed to read label map file: " << label_map_file;
      CHECK(MapLabelToName(label_map, true, &label_to_name_))
          << "Failed to convert label to name.";
      CHECK(MapLabelToDisplayName(label_map, true, &label_to_display_name_))
          << "Failed to convert label to display name.";
    }
  } else {
    need_save_ = false;
  }
  if (save_output_param.has_name_size_file()) {
    string name_size_file = save_output_param.name_size_file();
    if (name_size_file.empty()) {
      // Ignore saving if there is no name_size_file provided.
      LOG(WARNING) << "Provide name_size_file if output results to files.";
      need_save_ = false;
    } else {
      std::ifstream infile(name_size_file.c_str());
      CHECK(infile.good())
          << "Failed to open name size file: " << name_size_file;
      // The file is in the following format:
      //    name height width
      //    ...
      string name;
      int height, width;
      while (infile >> name >> height >> width) {
        names_.push_back(name);
        sizes_.push_back(std::make_pair(height, width));
      }
      infile.close();
      if (save_output_param.has_num_test_image()) {
        num_test_image_ = save_output_param.num_test_image();
      } else {
        num_test_image_ = names_.size();
      }
      CHECK_LE(num_test_image_, names_.size());
    }
  } else {
    need_save_ = false;
  }
  has_resize_ = save_output_param.has_resize_param();
  if (has_resize_) {
    resize_param_ = save_output_param.resize_param();
  }
  name_count_ = 0;
  visualize_ = detection_output_param.visualize();
  if (visualize_) {
    visualize_threshold_ = 0.6;
    if (detection_output_param.has_visualize_threshold()) {
      visualize_threshold_ = detection_output_param.visualize_threshold();
    }
    data_transformer_.reset(
        new DataTransformer<Dtype>(this->layer_param_.transform_param(),
                                   this->phase_));
    data_transformer_->InitRand();
    save_file_ = detection_output_param.save_file();
  }
  bbox_preds_.ReshapeLike(*(bottom[0]));
  if (!share_location_) {
    bbox_permute_.ReshapeLike(*(bottom[0]));
  }
  conf_permute_.ReshapeLike(*(bottom[1]));
}

template <typename Dtype>
void DetectionOutputLayer<Dtype>::Reshape(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
  if (need_save_) {
    CHECK_LE(name_count_, names_.size());
    if (name_count_ % num_test_image_ == 0) {
      // Clean all outputs.
      if (output_format_ == "VOC") {
        boost::filesystem::path output_directory(output_directory_);
        for (map<int, string>::iterator it = label_to_name_.begin();
             it != label_to_name_.end(); ++it) {
          if (it->first == background_label_id_) {
            continue;
          }
          std::ofstream outfile;
          boost::filesystem::path file(
              output_name_prefix_ + it->second + ".txt");
          boost::filesystem::path out_file = output_directory / file;
          outfile.open(out_file.string().c_str(), std::ofstream::out);
        }
      }
    }
  }
  CHECK_EQ(bottom[0]->num(), bottom[1]->num());
  if (bbox_preds_.num() != bottom[0]->num() ||
      bbox_preds_.count(1) != bottom[0]->count(1)) {
    bbox_preds_.ReshapeLike(*(bottom[0]));
  }
  if (!share_location_ && (bbox_permute_.num() != bottom[0]->num() ||
      bbox_permute_.count(1) != bottom[0]->count(1))) {
    bbox_permute_.ReshapeLike(*(bottom[0]));
  }
  if (conf_permute_.num() != bottom[1]->num() ||
      conf_permute_.count(1) != bottom[1]->count(1)) {
    conf_permute_.ReshapeLike(*(bottom[1]));
  }
  num_priors_ = bottom[2]->height() / 4;
  CHECK_EQ(num_priors_ * num_loc_classes_ * 4, bottom[0]->channels())
      << "Number of priors must match number of location predictions.";
  CHECK_EQ(num_priors_ * num_classes_, bottom[1]->channels())
      << "Number of priors must match number of confidence predictions.";
  if(attri_type_ == DetectionOutputParameter_AnnoataionAttriType_FACE){
    if (blur_permute_.num() != bottom[3]->num() ||
      blur_permute_.count(1) != bottom[3]->count(1)) {
    blur_permute_.ReshapeLike(*(bottom[3]));
    }
    if (occlu_permute_.num() != bottom[4]->num() ||
        occlu_permute_.count(1) != bottom[4]->count(1)) {
      occlu_permute_.ReshapeLike(*(bottom[4]));
    }
    CHECK_EQ(num_priors_ * num_blur_, bottom[3]->channels())
        << "Number of priors must match number of blur predictions.";
    CHECK_EQ(num_priors_ * num_occlusion_, bottom[4]->channels())
        << "Number of priors must match number of occlussion predictions.";
    // num() and channels() are 1.
    vector<int> top_shape(2, 1);
    // Since the number of bboxes to be kept is unknown before nms, we manually
    // set it to (fake) 1.
    top_shape.push_back(1);
    // Each row is a 9 dimension vector, which stores
    // [image_id, label, confidence, xmin, ymin, xmax, ymax, blur, occlussion]
    top_shape.push_back(9);
    top[0]->Reshape(top_shape);
  }else if(attri_type_ == DetectionOutputParameter_AnnoataionAttriType_LPnumber){
    CHECK_EQ(num_priors_ * num_chinese_, bottom[3]->channels())
    << "Number of priors must match number of chinese predictions.";
    CHECK_EQ(num_priors_ * num_english_, bottom[4]->channels())
    << "Number of priors must match number of english predictions.";
    CHECK_EQ(num_priors_ * num_letter_, bottom[5]->channels())
    << "Number of priors must match number of letter predictions.";
    vector<int> top_shape(2, 1);
    // Since the number of bboxes to be kept is unknown before nms, we manually
    // set it to (fake) 1.
    top_shape.push_back(1);
    // Each row is a 14 dimension vector, which stores
    // [image_id, label, confidence, xmin, ymin, xmax, ymax, 7-lpnumber]
    top_shape.push_back(14);
    top[0]->Reshape(top_shape);
  }  
}

template <typename Dtype>
void DetectionOutputLayer<Dtype>::Forward_cpu(
    const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {
  const Dtype* loc_data = bottom[0]->cpu_data();
  const Dtype* conf_data = bottom[1]->cpu_data();
  const Dtype* prior_data = bottom[2]->cpu_data();
  const int num = bottom[0]->num();
  // Retrieve all location predictions.
  vector<LabelBBox> all_loc_preds;
  GetLocPredictions(loc_data, num, num_priors_, num_loc_classes_,
                    share_location_, &all_loc_preds);
  // Retrieve all confidences.
  vector<map<int, vector<float> > > all_conf_scores;
  GetConfidenceScores(conf_data, num, num_priors_, num_classes_,
                      &all_conf_scores);
  
  // Retrieve all prior bboxes. It is same within a batch since we assume all
  // images in a batch are of same dimension.
  vector<NormalizedBBox> prior_bboxes;
  vector<vector<float> > prior_variances;
  GetPriorBBoxes(prior_data, num_priors_, &prior_bboxes, &prior_variances);

  // Decode all loc predictions to bboxes.
  vector<LabelBBox> all_decode_bboxes;
  const bool clip_bbox = false;
  DecodeBBoxesAll(all_loc_preds, prior_bboxes, prior_variances, num,
                  share_location_, num_loc_classes_, background_label_id_,
                  code_type_, variance_encoded_in_target_, clip_bbox,
                  &all_decode_bboxes);

  int num_kept = 0;
  vector<map<int, vector<int> > > all_indices;
  for (int i = 0; i < num; ++i) {
    const LabelBBox& decode_bboxes = all_decode_bboxes[i];
    const map<int, vector<float> >& conf_scores = all_conf_scores[i];
    map<int, vector<int> > indices;
    int num_det = 0;
    for (int c = 0; c < num_classes_; ++c) {
      if (c == background_label_id_) {
        // Ignore background class.
        continue;
      }
      if (conf_scores.find(c) == conf_scores.end()) {
        // Something bad happened if there are no predictions for current label.
        LOG(FATAL) << "Could not find confidence predictions for label " << c;
      }
      const vector<float>& scores = conf_scores.find(c)->second;
      int label = share_location_ ? -1 : c;
      if (decode_bboxes.find(label) == decode_bboxes.end()) {
        // Something bad happened if there are no predictions for current label.
        LOG(FATAL) << "Could not find location predictions for label " << label;
        continue;
      }
      const vector<NormalizedBBox>& bboxes = decode_bboxes.find(label)->second;
      ApplyNMSFast(bboxes, scores, confidence_threshold_, nms_threshold_, eta_,
          top_k_, &(indices[c]));
      num_det += indices[c].size();
    }
    if (keep_top_k_ > -1 && num_det > keep_top_k_) {
      vector<pair<float, pair<int, int> > > score_index_pairs;
      for (map<int, vector<int> >::iterator it = indices.begin();
           it != indices.end(); ++it) {
        int label = it->first;
        const vector<int>& label_indices = it->second;
        if (conf_scores.find(label) == conf_scores.end()) {
          // Something bad happened for current label.
          LOG(FATAL) << "Could not find location predictions for " << label;
          continue;
        }
        const vector<float>& scores = conf_scores.find(label)->second;
        for (int j = 0; j < label_indices.size(); ++j) {
          int idx = label_indices[j];
          CHECK_LT(idx, scores.size());
          score_index_pairs.push_back(std::make_pair(
                  scores[idx], std::make_pair(label, idx)));
        }
      }
      // Keep top k results per image.
      std::sort(score_index_pairs.begin(), score_index_pairs.end(),
                SortScorePairDescend<pair<int, int> >);
      score_index_pairs.resize(keep_top_k_);
      // Store the new indices.
      map<int, vector<int> > new_indices;
      for (int j = 0; j < score_index_pairs.size(); ++j) {
        int label = score_index_pairs[j].second.first;
        int idx = score_index_pairs[j].second.second;
        new_indices[label].push_back(idx);
      }
      all_indices.push_back(new_indices);
      num_kept += keep_top_k_;
    } else {
      all_indices.push_back(indices);
      num_kept += num_det;
    }
  }
  vector<int> top_shape(2, 1);
  top_shape.push_back(num_kept);
  if(attri_type_ == DetectionOutputParameter_AnnoataionAttriType_FACE){
    const Dtype* blur_data = bottom[3]->cpu_data();
    const Dtype* occlu_data = bottom[4]->cpu_data();

    //Retrieve all blur_data confidences.
    vector<map<int, vector<float> > > all_blur_scores;
    GetConfidenceScores(blur_data, num, num_priors_, num_blur_,
                      &all_blur_scores);
    //Retrieve all blur_data confidences.
    vector<map<int, vector<float> > > all_occlu_scores;
    GetConfidenceScores(occlu_data, num, num_priors_, num_occlusion_,
                      &all_occlu_scores);
    top_shape.push_back(9);
    Dtype* top_data;
    if (num_kept == 0) {
      LOG(INFO) << "Couldn't find any detections";
      top_shape[2] = num;
      top[0]->Reshape(top_shape);
      top_data = top[0]->mutable_cpu_data();
      caffe_set<Dtype>(top[0]->count(), -1, top_data);
      // Generate fake results per image.
      for (int i = 0; i < num; ++i) {
        top_data[0] = i;
        top_data += 9;
      }
    } else {
      top[0]->Reshape(top_shape);
      top_data = top[0]->mutable_cpu_data();
    }
    int count = 0;
    boost::filesystem::path output_directory(output_directory_);
    for (int i = 0; i < num; ++i) {
      const map<int, vector<float> >& conf_scores = all_conf_scores[i];
      const map<int, vector<float> >& blur_scores = all_blur_scores[i];
      const map<int, vector<float> >& occlu_scores = all_occlu_scores[i];
      const LabelBBox& decode_bboxes = all_decode_bboxes[i];
      for (map<int, vector<int> >::iterator it = all_indices[i].begin();
          it != all_indices[i].end(); ++it) {
        int label = it->first;
        if (conf_scores.find(label) == conf_scores.end()) {
          // Something bad happened if there are no predictions for current label.
          LOG(FATAL) << "Could not find confidence predictions for " << label;
          continue;
        }
        const vector<float>& scores = conf_scores.find(label)->second;
        int loc_label = share_location_ ? -1 : label;
        if (decode_bboxes.find(loc_label) == decode_bboxes.end()) {
          // Something bad happened if there are no predictions for current label.
          LOG(FATAL) << "Could not find location predictions for " << loc_label;
          continue;
        }
        const vector<NormalizedBBox>& bboxes =
            decode_bboxes.find(loc_label)->second;
        vector<int>& indices = it->second;
        if (need_save_) {
          CHECK(label_to_name_.find(label) != label_to_name_.end())
            << "Cannot find label: " << label << " in the label map.";
          CHECK_LT(name_count_, names_.size());
        }
        for (int j = 0; j < indices.size(); ++j) {
          int idx = indices[j];
          top_data[count * 9] = i;
          top_data[count * 9 + 1] = label;
          top_data[count * 9 + 2] = scores[idx];
          const NormalizedBBox& bbox = bboxes[idx];
          top_data[count * 9 + 3] = bbox.xmin();
          top_data[count * 9 + 4] = bbox.ymin();
          top_data[count * 9 + 5] = bbox.xmax();
          top_data[count * 9 + 6] = bbox.ymax();
          int blur_index = 0; int occlu_index = 0;
          float blur_temp =0; float occlu_temp =0.0;
          for (int ii = 0; ii< 3; ii++ )
          {
            if (blur_temp <  blur_scores.find(ii)->second[idx])
            {
              blur_index = ii;
              blur_temp = blur_scores.find(ii)->second[idx];
            }
            if (occlu_temp <  occlu_scores.find(ii)->second[idx])
            {
              occlu_index = ii;
              occlu_temp = occlu_scores.find(ii)->second[idx];
            }
          }
          top_data[count * 9 + 7] = blur_index;
          top_data[count * 9 + 8] = occlu_index;
          ++count;
        }
      }
    }
  }else if(attri_type_ == DetectionOutputParameter_AnnoataionAttriType_LPnumber){
    const Dtype* chi_data = bottom[3]->cpu_data();
    const Dtype* eng_data = bottom[4]->cpu_data();
    const Dtype* let1_data = bottom[5]->cpu_data();
    const Dtype* let2_data = bottom[6]->cpu_data();
    const Dtype* let3_data = bottom[7]->cpu_data();
    const Dtype* let4_data = bottom[8]->cpu_data();
    const Dtype* let5_data = bottom[9]->cpu_data();
    const int num = bottom[0]->num();
    //Retrieve all chi_data confidences.
    vector<map<int, vector<float> > > all_chi_scores;
    GetConfidenceScores(chi_data, num, num_priors_, num_chinese_,
                      &all_chi_scores);
    //Retrieve all eng_data confidences
    vector<map<int, vector<float> > > all_eng_scores;
    GetConfidenceScores(eng_data, num, num_priors_, num_english_,
                        &all_eng_scores);
    //Retrieve all let1_data confidences
    vector<vector<map<int, vector<float> > > > all_let_scores(5);
    GetConfidenceScores(let1_data, num, num_priors_, num_letter_,
                        &all_let_scores[0]);
    //Retrieve all let2_data confidences
    GetConfidenceScores(let2_data, num, num_priors_, num_letter_,
                        &all_let_scores[1]);
    //Retrieve all let3_data confidences
    GetConfidenceScores(let3_data, num, num_priors_, num_letter_,
                        &all_let_scores[2]);
    //Retrieve all let4_data confidences
    GetConfidenceScores(let4_data, num, num_priors_, num_letter_,
                        &all_let_scores[3]);
    //Retrieve all let5_data confidences
    GetConfidenceScores(let5_data, num, num_priors_, num_letter_,
                        &all_let_scores[4]);
    top_shape.push_back(14);
    Dtype* top_data;
    if (num_kept == 0) {
      LOG(INFO) << "Couldn't find any detections";
      top_shape[2] = num;
      top[0]->Reshape(top_shape);
      top_data = top[0]->mutable_cpu_data();
      caffe_set<Dtype>(top[0]->count(), -1, top_data);
      // Generate fake results per image.
      for (int i = 0; i < num; ++i) {
        top_data[0] = i;
        top_data += 14;
      }
    } else {
      top[0]->Reshape(top_shape);
      top_data = top[0]->mutable_cpu_data();
    }
    int count = 0;
    boost::filesystem::path output_directory(output_directory_);
    for (int i = 0; i < num; ++i) {
      const map<int, vector<float> >& conf_scores = all_conf_scores[i];
      const map<int, vector<float> >& chi_scores = all_chi_scores[i];
      const map<int, vector<float> >& eng_scores = all_eng_scores[i];
      const LabelBBox& decode_bboxes = all_decode_bboxes[i];
      for (map<int, vector<int> >::iterator it = all_indices[i].begin();
          it != all_indices[i].end(); ++it) {
        int label = it->first;
        if (conf_scores.find(label) == conf_scores.end()) {
          // Something bad happened if there are no predictions for current label.
          LOG(FATAL) << "Could not find confidence predictions for " << label;
          continue;
        }
        const vector<float>& scores = conf_scores.find(label)->second;
        int loc_label = share_location_ ? -1 : label;
        if (decode_bboxes.find(loc_label) == decode_bboxes.end()) {
          // Something bad happened if there are no predictions for current label.
          LOG(FATAL) << "Could not find location predictions for " << loc_label;
          continue;
        }
        const vector<NormalizedBBox>& bboxes =
            decode_bboxes.find(loc_label)->second;
        vector<int>& indices = it->second;
        if (need_save_) {
          CHECK(label_to_name_.find(label) != label_to_name_.end())
            << "Cannot find label: " << label << " in the label map.";
          CHECK_LT(name_count_, names_.size());
        }
        for (int j = 0; j < indices.size(); ++j) {
          int idx = indices[j];
          top_data[count * 14] = i;
          top_data[count * 14 + 1] = label;
          top_data[count * 14 + 2] = scores[idx];
          const NormalizedBBox& bbox = bboxes[idx];
          top_data[count * 14 + 3] = bbox.xmin();
          top_data[count * 14 + 4] = bbox.ymin();
          top_data[count * 14 + 5] = bbox.xmax();
          top_data[count * 14 + 6] = bbox.ymax();
          int let_index[5] ={0};
          float let_temp[5] ={0.0f};
          int chi_index = 0; int eng_index = 0;
          float chi_temp =0; float eng_temp =0.0; 
          for (int ii = 0; ii< num_chinese_; ii++ )
          {
            if (chi_temp <  chi_scores.find(ii)->second[idx])
            {
              chi_index = ii;
              chi_temp = chi_scores.find(ii)->second[idx];
            }
          }
          for (int ii = 0; ii< num_english_; ii++ )
          {
            if (eng_temp <  eng_scores.find(ii)->second[idx])
            {
              eng_index = ii;
              eng_temp = eng_scores.find(ii)->second[idx];
            }
          }
          for(int jj = 0 ; jj < 5; jj++)
          {
            for (int ii = 0; ii< num_letter_; ii++ )
            {
              if (let_temp[jj] < all_let_scores[jj][i].find(ii)->second[idx])
              {
                let_index[jj] = ii;
                let_temp[jj] = all_let_scores[jj][i].find(ii)->second[idx];
              }
            }
          }
          top_data[count * 14 + 7] = chi_index;
          top_data[count * 14 + 8] = eng_index;
          top_data[count * 14 + 9] = let_index[0];
          top_data[count * 14 + 10] = let_index[1];
          top_data[count * 14 + 11] = let_index[2];
          top_data[count * 14 + 12] = let_index[3];
          top_data[count * 14 + 13] = let_index[4];
          ++count;
        }
      }
    }
  }
  if (visualize_) {
#ifdef USE_OPENCV
    vector<cv::Mat> cv_imgs;
    this->data_transformer_->TransformInv(bottom[3], &cv_imgs);
    vector<cv::Scalar> colors = GetColors(label_to_display_name_.size());
    VisualizeBBox(cv_imgs, top[0], visualize_threshold_, colors,
        label_to_display_name_, save_file_);
#endif  // USE_OPENCV
  }
}

#ifdef CPU_ONLY
STUB_GPU_FORWARD(DetectionOutputLayer, Forward);
#endif

INSTANTIATE_CLASS(DetectionOutputLayer);
REGISTER_LAYER_CLASS(DetectionOutput);

}  // namespace caffe
