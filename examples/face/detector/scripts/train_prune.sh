#!/bin/sh
if ! test -f ../prototxt/model/face_train.prototxt ;then
	echo "error: ../prototxt/model/face_train.prototxt does not exit."
	echo "please generate your own model prototxt primarily."
        exit 1
fi
if ! test -f ../prototxt/model/face_test.prototxt ;then
	echo "error: ../prototxt/model/face_test.prototxt does not exit."
	echo "please generate your own model prototxt primarily."
        exit 1
fi
../../../../build/tools/caffe train --solver=../solver/solver_train_prune.prototxt -gpu 0 \
--weights ../snapshot/face_iter_80000.caffemodel
#--snapshot=../snapshot/face_iter_100000.solverstate

