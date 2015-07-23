/*
 
 ofxCaffe - testApp.h
 
 The MIT License (MIT)
 
 Copyright (c) 2015 Parag K. Mital, http://pkmital.com
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 
 
 */

#pragma once

//#define DO_LOAD_VIDEO 1
#define DO_LOAD_SYPHON 1
//#define DO_LOAD_CAMERA 1

#include "ofMain.h"
#include "ofxOpenCv.h"
#include "ofxGui.h"
#include "ofxCaffe.hpp"
#include "adaptive_manifold_filter.hpp"


class synthesisThread : public ofThread {
public:
    synthesisThread()
    {
        b_initialized = false;
        b_mutex = false;
        
        current_model = 0;
        
        b_0 = b_1 = true;
        b_2 = b_3 = b_4 = false;
        
        caffe = std::shared_ptr<ofxCaffe>(new ofxCaffe());
        caffe->initModel(ofxCaffe::getModelTypes()[current_model]);
        cout << "LAYERS: " << endl;
        caffe->printLayerNames();
        cout << "BLOBS: " << endl;
        caffe->printBlobNames();
        
        int layer = current_model == 0 ? 3 : 117;
        gui.setup();
        gui.add(iterations.setup("Iterations", 5, 0, 20));
        gui.add(l1_norm.setup("L1 Norm", 0.1, 0.0, 1.0));
        gui.add(l2_norm.setup("L2 Norm", 0.1, 0.0, 1.0));
        gui.add(stepsize.setup("Step Size", 5.5, 0.0, 10.0));
        gui.add(interp.setup("Input Mix", 0.1, 0.0, 1.0));
        gui.add(zoom.setup("Zoom", 1.00, 0.0, 2.0));
        gui.add(rotation.setup("Rotation", 0.0, 0.0, 360.0));
        gui.add(layer_num.setup("Layer Num", layer, 0, caffe->getBlobNames().size()));
        gui.add(octaves.setup("Octaves", 5, 0, 16));
        gui.add(octave_scale.setup("Octave Scale", 1.5, 0.1, 3.0));
        gui.add(gradient_clip.setup("Gradient Clip", 10.0, 0.0, 10.0));
        gui.add(grayscale_amount.setup("Grayscale", 0.5, 0.0, 1.0));
        gui.add(jitter.setup("Jitter", 32, 0, 128));
        gui.add(neuron.setup("Neuron", -1, 0, caffe->getNumberOfNeurons(caffe->getBlobNames()[layer])));
        gui.add(b_filter.setup("Filter", false));
        
        filter = cv::AdaptiveManifoldFilter::create();
        filter->set("sigma_s", 4.0);
        filter->set("sigma_r", 0.15);
        filter->set("tree_height", -1);
        filter->set("num_pca_iterations", 1);
        
        startThread(true);
    }
    
    ~synthesisThread()
    {
        waitForThread(true);
    }
    
    void allocate(int w, int h)
    {
        width = w;
        height = h;
        
        dst_rgb = cv::Mat(cv::Size(w, h), CV_8UC3);
    }
    
    void update(const cv::Mat& src_rgb)
    {
        if(!b_initialized)
        {
            if(b_filter)
            {
                cv::Mat dst, tilde_dst, joint_img;
                filter->apply(src_rgb, src_rgb, tilde_dst, joint_img);
            }
            
            cv::Mat combined_img = dst_rgb * interp + src_rgb * (1.0 - interp);
            combined_img.copyTo(this->src_rgb);
            b_initialized = true;
        }
    }
    
    void getLatestIteration(cv::Mat &rgb)
    {
        dst_rgb.copyTo(rgb);
    }
    
    void draw(int x, int y, int w, int h)
    {
        caffe->drawAmplification(x, y, w, h);
        
        if (caffe->getModelType() == ofxCaffe::OFXCAFFE_MODEL_BVLC_CAFFENET_34x17 ||
            caffe->getModelType() == ofxCaffe::OFXCAFFE_MODEL_BVLC_CAFFENET_8x8)
        {
            ofEnableAlphaBlending();
            ofSetColor(255, 255, 255, 200);
            caffe->drawDetectionGrid(w, h);
            ofDisableAlphaBlending();
        }
        
        gui.draw();
        
//        ofDrawBitmapStringHighlight("model (-/+):  " + caffe->getModelTypeNames()[current_model], 20, h - 70);
        
        ofDisableAlphaBlending();
//        if (b_1)
//            caffe->drawLabelAt(20, h - 50);
        if (b_2)
            caffe->drawLayerXParams(0, 80, w, 32, layer_num, ofGetElapsedTimef() * 10.0);
        if (b_3)
            caffe->drawLayerXOutput(0, 420, w, 32, layer_num);
//        if (b_4)
//            caffe->drawProbabilities(0, 500, w, 200);
    }
    
    void keyPressed(int key)
    {
        if (key == '0')
            b_0 = !b_0;
        else if (key == '1')
            b_1 = !b_1;
        else if (key == '2')
            b_2 = !b_2;
        else if (key == '3')
            b_3 = !b_3;
        else if (key == '4')
            b_4 = !b_4;
        else if (key == '[')
        {
            layer_num = std::max<int>(0, layer_num - 1);
            cout << "layer_num '[' or ']': " << layer_num << ": " << caffe->getLayerNames()[layer_num] << endl;
        }
        else if (key == ']')
        {
            while (b_mutex) {} b_mutex = true;
            layer_num = std::min<int>(caffe->getTotalNumBlobs(), layer_num + 1);
            b_mutex = false;
            cout << "layer_num '[' or ']': " << layer_num << ": " << caffe->getLayerNames()[layer_num] << endl;
        }
        else if (key == '-' || key == '_')
        {
            current_model = (current_model == 0) ? (ofxCaffe::getTotalModelNums() - 1)  : (current_model - 1);
            while (b_mutex) {} b_mutex = true;
            caffe.reset();
            caffe = std::shared_ptr<ofxCaffe>(new ofxCaffe());
            caffe->initModel(ofxCaffe::getModelTypes()[current_model]);
            b_mutex = false;
        }
        else if (key == '+' || key == '=')
        {
            current_model = (current_model + 1) % ofxCaffe::getTotalModelNums();
            while (b_mutex) {} b_mutex = true;
            caffe.reset();
            caffe = std::shared_ptr<ofxCaffe>(new ofxCaffe());
            caffe->initModel(ofxCaffe::getModelTypes()[current_model]);
            b_mutex = false;
        }
    }
    
protected:
    void threadedFunction() {
        while(isThreadRunning())
        {
            if(b_initialized)
            {
                caffe->amplifyLayer(src_rgb,
                                    dst_rgb,
                                    caffe->getBlobNames()[layer_num],
                                    l1_norm,
                                    l2_norm,
                                    stepsize,
                                    octaves,
                                    octave_scale,
                                    gradient_clip,
                                    jitter,
                                    iterations,
                                    true,
                                    grayscale_amount,
                                    neuron);
                
                dst_rgb.copyTo(src_rgb);
                b_initialized = false;
            }
        }
    }
    
    //--------------------------------------------------------------
    // ptr to caffe obj
    std::shared_ptr<ofxCaffe> caffe;
    
    cv::Mat src_rgb, dst_rgb;
    
    std::mutex mutex;
    
    //--------------------------------------------------------------
    // Denoising
    cv::Ptr<cv::AdaptiveManifoldFilter>   filter;
    
    //--------------------------------------------------------------
    // simple flags for switching on drawing options of camera image/layers/parameters/probabilities
    bool b_0, b_1, b_2, b_3, b_4;
    
    //--------------------------------------------------------------
    // which model have we loaded
    int current_model;
    
    //--------------------------------------------------------------
    // which layer are we visualizing
    ofxIntSlider layer_num;
    
    //--------------------------------------------------------------
    // image and window dimensions
    int width, height;
    
    bool b_mutex;
    
    ofxPanel gui;
    ofxFloatSlider l1_norm;
    ofxFloatSlider l2_norm;
    ofxFloatSlider grayscale_amount;
    ofxFloatSlider stepsize;
    ofxFloatSlider zoom;
    ofxFloatSlider rotation;
    ofxFloatSlider interp;
    ofxIntSlider octaves;
    ofxIntSlider iterations;
    ofxFloatSlider octave_scale;
    ofxFloatSlider gradient_clip;
    ofxIntSlider jitter;
    ofxIntSlider neuron;
    ofxToggle b_filter;
    
    bool b_initialized;
};

#include "ofxSyphonClient.h"

class testApp : public ofBaseApp{

public:
    //--------------------------------------------------------------
    void setup();
    void update();
    void draw();
    
    //--------------------------------------------------------------
    void keyPressed(int key);
    void keyReleased(int key);
    void mouseMoved(int x, int y );
    void mouseDragged(int x, int y, int button);
    void mousePressed(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void windowResized(int w, int h);
    void dragEvent(ofDragInfo dragInfo);
    void gotMessage(ofMessage msg);
	
    //--------------------------------------------------------------
    // camera and opencv image objects
#ifdef DO_LOAD_VIDEO
    ofVideoPlayer video;
    int current_frame, total_frames;
#else
#ifdef DO_LOAD_SYPHON
    ofxSyphonClient syphon;
    ofFbo fbo;
    ofPixels pixels;
#else
    ofVideoGrabber camera;
#endif
#endif
    
    ofxCvColorImage camera_img, camera_img_rsz, synthesis_img;
    ofxCvGrayscaleImage synthesis_h, synthesis_s, synthesis_v;
    
    //--------------------------------------------------------------
    // ptr to caffe obj
    synthesisThread caffe;
    
    //--------------------------------------------------------------
    // image and window dimensions
    int width, height;
    int width_og, height_og;
    
    //--------------------------------------------------------------
    // hacky mutex for when changing caffe model
    bool b_setup;
    
};
