#include <cmath>
#include <vector>

#include "caffe/layers/swish_layer.hpp"

namespace caffe {

template <typename Dtype>
inline Dtype sigmoid(Dtype x) {
  return 1. / (1. + exp(-x));
} 
template <typename Dtype>
inline Dtype swish(Dtype x, Dtype beta) {
  return sigmoid(beta * x) * x;
}

template <typename Dtype>
void SwishLayer<Dtype>::Forward_cpu(const vector<Blob<Dtype>*>& bottom,
    const vector<Blob<Dtype>*>& top) {
  const Dtype* bottom_data = bottom[0]->cpu_data();
  Dtype* top_data = top[0]->mutable_cpu_data();
  const int count = bottom[0]->count();
  for (int i = 0; i < count; ++i) {
    top_data[i] = swish(bottom_data[i], Dtype(1.0));
  }
}

template <typename Dtype>
void SwishLayer<Dtype>::Backward_cpu(const vector<Blob<Dtype>*>& top,
    const vector<bool>& propagate_down,
    const vector<Blob<Dtype>*>& bottom) {
  if (propagate_down[0]) {
    const Dtype* top_diff = top[0]->cpu_diff();
    Dtype* bottom_diff = bottom[0]->mutable_cpu_diff();
    const Dtype* bottom_data = bottom[0]->cpu_data();
    const int count = bottom[0]->count();
    for (int i = 0; i < count; ++i) {
      const Dtype sigmoid_x = sigmoid(bottom_data[i]);
      bottom_diff[i] = top_diff[i] * (bottom_data[i] * sigmoid_x * (1. - sigmoid_x) + sigmoid_x);
    }
  }
}

#ifdef CPU_ONLY
STUB_GPU(SwishLayer);
#endif

INSTANTIATE_CLASS(SwishLayer);


}  // namespace caffe
