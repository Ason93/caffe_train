caffe train face licenseplate reID action ocr  
focal loss layer implemented by caffe  
cosin face loss layer implemented by caffe  
data augement for pymraidBox implemented by caffe  
yolo layer & reorg layer for darknet implemented by caffe  
centernet implemented by caffe  
facenet tripletloss by caffe  
centernet face + nms version  
widerface val set  
hard mid easy  
72% 84% 85%  
mobilenet-v2 face vggface val accuray  
99.42%  
face landmarks + face attributes gender (99.4%)+ bool glasses(99.5%)  
face head angle(not evaluated)  
car license plate detect(mAP91%) + recognition(96%, only support anhui car + blue car style)  
New proposed method CenterGridSoftmax + nms method by using anchor  
widerface val set  
hard mid easy  
72% 84% 86%  

will add caffe version efficientDet  
to be continued

import caffe
from caffe import layers as L
from caffe import params as P

python Caffe API
Data�㶨��
lmdb/leveldb Data�㶨��

L.Data( 
        source=lmdb,
        backend=P.Data.LMDB,
        batch_size=batch_size, ntop=2,
        transform_param=dict(
                              crop_size=227,
                              mean_value=[104, 117, 123],
                              mirror=True
                              )
        )

HDF5 Data�㶨��
L.HDF5Data(
            hdf5_data_param={
                            'source': './training_data_paths.txt',  
                            'batch_size': 64
                            },
            include={
                    'phase': caffe.TRAIN
                    }
            )

mageData Data�㶨��
L.ImageData(
                source=list_path,
                batch_size=batch_size,
                new_width=48,
                new_height=48,
                ntop=2,
                ransform_param=dict(crop_size=40,mirror=True)
                )

Convloution�㶨��
L.Convolution(  
                bottom, 
                kernel_size=ks, 
                stride=stride,
                num_output=nout, 
                pad=pad, 
                group=group
                )
LRN�㶨��
L.LRN(
        bottom, 
        local_size=5, 
        alpha=1e-4, 
        beta=0.75
        )
Activation�㶨��
L.ReLU(
        bottom, 
        in_place=True
        )
L.Pooling(
            bottom,
            pool=P.Pooling.MAX, 
            kernel_size=ks, 
            stride=stride
            )
FullConnect�㶨��
L.InnerProduct(
                bottom, 
                num_output=nout
                )
Dropout�㶨��
L.Dropout(
            bottom, 
            in_place=True
            )
Loss�㶨��
L.SoftmaxWithLoss(
                    bottom, 
                    label
                    )



������ǿЧ��ͼ

����ԭͼ������һ��640*480��ͼƬ���������ڰ��������ҷ�����ͼƬ�ߴ粢��û��mean subtract������������resize�������������ͼƬ����resize��300x300��������Ҫ��������ǿ��Ч����SSD�е�������ǿ��˳���ǣ�

DistortImage: �����Ҫ���޸�ͼƬ��brightness��contrast��saturation��hue��reordering channels����û�ı��ǩbbox

ExpandImage: �����Ҫ�ǽ�DistortImage��ͼƬ������0������չ����ǩbbox��ʱ�϶���ı䣬�������Ժڱߵ����Ͻ�Ϊԭ�����[0,1]��bbox�����ϽǺ����½����������ꡣ

BatchSampler: ��������ѡ��ͼ�ˣ�BatchSampler����Ҫ��GT�Ĵ��ڲŻ���Ч���������������˵ļ������ͼ��û�˾Ͳ�������sampled_bboxes�������޸����ӡ�sampled_bboxes��ֵ�������[0, 1]�����ɵ�bbox�����Һ�ĳ��gt_bboxes��IOU��[min, max]֮�䡣����proto�����max_sample����Ϊ1������ÿ��batch_sampler���ܻ���1��sampled_bbox�����ȡһ��sampled bbox���Ҳü�ͼƬ�ͱ�ǩ����ǩ�ü�Ҳ�ܺ��������Ҫͨ��ProjectBBox��ԭ����ϵ��ǩͶӰ���ü���ͼƬ��������ϵ�����꣬Ȼ����ClipBBox��[0,1]֮�䡣

Resize��������300x300�����ͼƬ������300x300����ǩ��Ҳ�����Է���������ѡ�

Crop��ԭ��data_transformer����crop�ģ��������������prototxt�У�Ĭ����ԭͼ ���Ծͺ�ûcropһ�������Ҫcrop�Ļ���ǩҲ�ǻ��֮ǰBatchSampler��������

������ǿ��data_anchor_sampler
��һ��ͼ���������ȡһ��sface
