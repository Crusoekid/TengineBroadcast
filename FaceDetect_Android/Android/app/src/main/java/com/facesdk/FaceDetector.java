package com.facesdk;

import android.util.Log;

import java.util.ArrayList;
import java.util.List;

public class FaceDetector {
    static {
        System.loadLibrary("FaceDetect");
    }

    public static native void init();

    public static native float[] detect(byte[] img, int w, int h);

    public static native void release();

    public static List<FaceInfo> detectByBytes(byte[] img, int w, int h){
        float[] data = detect(img, w, h);
        if(data != null && data.length % 5 == 0){
            int num = data.length / 5;
            List<FaceInfo> faceInfos = new ArrayList<>(num);
            for(int i = 0; i < num; i++){
                FaceInfo faceInfo = new FaceInfo();
                faceInfo.x1 = data[i * 5 + 0];
                faceInfo.y1 = data[i * 5 + 1];
                faceInfo.x2 = data[i * 5 + 2];
                faceInfo.y2 = data[i * 5 + 3];
                faceInfo.score = data[i * 5 + 4];
                faceInfos.add(faceInfo);
            }
            return faceInfos;
        }
        return null;
    }
}
