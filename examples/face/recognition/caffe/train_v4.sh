#!/bin/sh
if ! test -f ./prototxt/model/train.prototxt ;then
	echo "error: train.prototxt does not exit."
	echo "please generate your own model prototxt primarily."
        exit 1
fi
../../../../build/tools/caffe train --solver=./prototxt/model/solver.prototxt -gpu 2 \
#--snapshot=./face_recog/tiny_iter_148441.solverstate
