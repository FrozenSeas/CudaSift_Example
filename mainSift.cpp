//********************************************************//
// CUDA SIFT extractor by Marten Björkman aka Celebrandil //
//              celle @ csc.kth.se                       //
//********************************************************//  

#include <iostream>  
#include <cmath>
#include <iomanip>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "cudaImage.h"
#include "cudaSift.h"

struct StructPointerTest{
        int x;
        int y;
};

struct RetStruct{
	int numPts;
	float *x_pos;
	float *y_pos;
	float *m_x_pos;
	float *m_y_pos;
	float *match_error;
};

int ImproveHomography(SiftData &data, float *homography, int numLoops, float minScore, float maxAmbiguity, float thresh);
void PrintMatchData(SiftData &siftData1, SiftData &siftData2, CudaImage &img);
void MatchAll(SiftData &siftData1, SiftData &siftData2, float *homography);
extern "C"{
	SiftData* img_sift_process(int height1, int width1, uchar* data1);
	RetStruct *sift_process(int height1, int width1, uchar* data1,int height2, int width2, uchar* data2);
}
double ScaleUp(CudaImage &res, CudaImage &src);

///////////////////////////////////////////////////////////////////////////////
// Main program
///////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv) 
{    
  int devNum = 0, imgSet = 0;
  if (argc>1)
    devNum = std::atoi(argv[1]);
  if (argc>2)
    imgSet = std::atoi(argv[2]);

  // Read images using OpenCV
  cv::Mat limg, rimg;
  if (imgSet) {
    cv::imread("data/left.pgm", 0).convertTo(limg, CV_32FC1);
    cv::imread("data/righ.pgm", 0).convertTo(rimg, CV_32FC1);
  } else {
    cv::imread("data/img1.png", 0).convertTo(limg, CV_32FC1);
    cv::imread("data/img2.png", 0).convertTo(rimg, CV_32FC1);
  }
  //cv::flip(limg, rimg, -1);
  unsigned int w = limg.cols;
  unsigned int h = limg.rows;
  std::cout << "Image size = (" << w << "," << h << ")" << std::endl;
  
  // Initial Cuda images and download images to device
  std::cout << "Initializing data..." << std::endl;
  InitCuda(devNum); 
  CudaImage img1, img2;
  img1.Allocate(w, h, iAlignUp(w, 128), false, NULL, (float*)limg.data);
  img2.Allocate(w, h, iAlignUp(w, 128), false, NULL, (float*)rimg.data);
  img1.Download();
  img2.Download(); 

  // Extract Sift features from images
  SiftData siftData1, siftData2;
  float initBlur = 1.0f;
  float thresh = (imgSet ? 4.5f : 3.0f);
  InitSiftData(siftData1, 32768, true, true); 
  InitSiftData(siftData2, 32768, true, true);
  
  // A bit of benchmarking 
  //for (int thresh1=1.00f;thresh1<=4.01f;thresh1+=0.50f) {
  float *memoryTmp = AllocSiftTempMemory(w, h, 5, false);
    for (int i=0;i<1000;i++) {
      ExtractSift(siftData1, img1, 5, initBlur, thresh, 0.0f, false, memoryTmp);
      ExtractSift(siftData2, img2, 5, initBlur, thresh, 0.0f, false, memoryTmp);
    }
    FreeSiftTempMemory(memoryTmp);
    
    // Match Sift features and find a homography
    for (int i=0;i<1;i++)
      MatchSiftData(siftData1, siftData2);
    float homography[9];
    int numMatches;
    FindHomography(siftData1, homography, &numMatches, 10000, 0.00f, 0.80f, 5.0);
    int numFit = ImproveHomography(siftData1, homography, 5, 0.00f, 0.80f, 3.0);
    
    std::cout << "Number of original features: " <<  siftData1.numPts << " " << siftData2.numPts << std::endl;
    std::cout << "Number of matching features: " << numFit << " " << numMatches << " " << 100.0f*numFit/std::min(siftData1.numPts, siftData2.numPts) << "% " << initBlur << " " << thresh << std::endl;
    //}
  
  // Print out and store summary data
  PrintMatchData(siftData1, siftData2, img1);
  cv::imwrite("data/limg_pts.pgm", limg);

  //MatchAll(siftData1, siftData2, homography);
  
  // Free Sift data from device
  FreeSiftData(siftData1);
  FreeSiftData(siftData2);
  return 1;
}

void MatchAll(SiftData &siftData1, SiftData &siftData2, float *homography)
{
#ifdef MANAGEDMEM
  SiftPoint *sift1 = siftData1.m_data;
  SiftPoint *sift2 = siftData2.m_data;
#else
  SiftPoint *sift1 = siftData1.h_data;
  SiftPoint *sift2 = siftData2.h_data;
#endif
  int numPts1 = siftData1.numPts;
  int numPts2 = siftData2.numPts;
  int numFound = 0;
#if 1
  homography[0] = homography[4] = -1.0f;
  homography[1] = homography[3] = homography[6] = homography[7] = 0.0f;
  homography[2] = 1279.0f;
  homography[5] = 959.0f;
#endif
  for (int i=0;i<numPts1;i++) {
    float *data1 = sift1[i].data;
    std::cout << i << ":" << sift1[i].scale << ":" << (int)sift1[i].orientation << " " << sift1[i].xpos << " " << sift1[i].ypos << std::endl;
    bool found = false;
    for (int j=0;j<numPts2;j++) {
      float *data2 = sift2[j].data;
      float sum = 0.0f;
      for (int k=0;k<128;k++) {
	sum += data1[k]*data2[k];
	std::cout<<data1[k]<<' '<<data2[k]<<std::endl;
      }	
      float den = homography[6]*sift1[i].xpos + homography[7]*sift1[i].ypos + homography[8];
      float dx = (homography[0]*sift1[i].xpos + homography[1]*sift1[i].ypos + homography[2]) / den - sift2[j].xpos;
      float dy = (homography[3]*sift1[i].xpos + homography[4]*sift1[i].ypos + homography[5]) / den - sift2[j].ypos;
      float err = dx*dx + dy*dy;
      if (err<100.0f) // 100.0
	found = true;
      if (err<100.0f || j==sift1[i].match) { // 100.0
	if (j==sift1[i].match && err<100.0f)
	  std::cout << " *";
	else if (j==sift1[i].match) 
	  std::cout << " -";
	else if (err<100.0f)
	  std::cout << " +";
	else
	  std::cout << "  ";
	std::cout << j << ":" << sum << ":" << (int)sqrt(err) << ":" << sift2[j].scale << ":" << (int)sift2[j].orientation << " " << sift2[j].xpos << " " << sift2[j].ypos << " " << (int)dx << " " << (int)dy << std::endl;
      }
    }
    std::cout << std::endl;
    if (found)
      numFound++;
  }
  std::cout << "Number of finds: " << numFound << " / " << numPts1 << std::endl;
  std::cout << homography[0] << " " << homography[1] << " " << homography[2] << std::endl;//%%%
  std::cout << homography[3] << " " << homography[4] << " " << homography[5] << std::endl;//%%%
  std::cout << homography[6] << " " << homography[7] << " " << homography[8] << std::endl;//%%%
}

void PrintMatchData(SiftData &siftData1, SiftData &siftData2, CudaImage &img)
{
  int numPts = siftData1.numPts;
#ifdef MANAGEDMEM
  SiftPoint *sift1 = siftData1.m_data;
  SiftPoint *sift2 = siftData2.m_data;
#else
  SiftPoint *sift1 = siftData1.h_data;
  SiftPoint *sift2 = siftData2.h_data;
#endif
  float *h_img = img.h_data;
  int w = img.width;
  int h = img.height;
  std::cout << std::setprecision(3);
  for (int j=0;j<numPts;j++) { 
    int k = sift1[j].match;
    if (sift1[j].match_error<5) {
      float dx = sift2[k].xpos - sift1[j].xpos;
      float dy = sift2[k].ypos - sift1[j].ypos;
#if 0
      if (false && sift1[j].xpos>550 && sift1[j].xpos<600) {
	std::cout << "pos1=(" << (int)sift1[j].xpos << "," << (int)sift1[j].ypos << ") ";
	std::cout << j << ": " << "score=" << sift1[j].score << "  ambiguity=" << sift1[j].ambiguity << "  match=" << k << "  ";
	std::cout << "scale=" << sift1[j].scale << "  ";
	std::cout << "error=" << (int)sift1[j].match_error << "  ";
	std::cout << "orient=" << (int)sift1[j].orientation << "," << (int)sift2[k].orientation << "  ";
	std::cout << " delta=(" << (int)dx << "," << (int)dy << ")" << std::endl;
      }
#endif
#if 1
      int len = (int)(fabs(dx)>fabs(dy) ? fabs(dx) : fabs(dy));
      for (int l=0;l<len;l++) {
	int x = (int)(sift1[j].xpos + dx*l/len);
	int y = (int)(sift1[j].ypos + dy*l/len);
	h_img[y*w+x] = 255.0f;
      }
#endif
    }
    int x = (int)(sift1[j].xpos+0.5);
    int y = (int)(sift1[j].ypos+0.5);
    int s = std::min(x, std::min(y, std::min(w-x-2, std::min(h-y-2, (int)(1.41*sift1[j].scale)))));
    int p = y*w + x;
    p += (w+1);
    for (int k=0;k<s;k++) 
      h_img[p-k] = h_img[p+k] = h_img[p-k*w] = h_img[p+k*w] = 0.0f;
    p -= (w+1);
    for (int k=0;k<s;k++) 
      h_img[p-k] = h_img[p+k] = h_img[p-k*w] =h_img[p+k*w] = 255.0f;
  }
  std::cout << std::setprecision(6);
}
RetStruct* sift_process(int height1, int width1, uchar* data1,int height2, int width2, uchar* data2)
{
  int devNum = 0, imgSet = 0;
  // Read images using OpenCV

  cv::Mat limg(height1, width1, CV_8UC1, data1);
  cv::Mat rimg(height2, width2, CV_8UC1, data2);
  std::cout<<limg.size()<<limg.channels()<<rimg.size()<<rimg.channels();
  //cv::flip(limg, rimg, -1);
  unsigned int w = limg.cols;
  unsigned int h = limg.rows;
  std::cout << "Image size = (" << w << "," << h << ")" << std::endl;

  // Initial Cuda images and download images to device
  std::cout << "Initializing data..." << std::endl;
  InitCuda(devNum);
  CudaImage img1, img2;
  img1.Allocate(w, h, iAlignUp(w, 128), false, NULL, (float*)limg.data);
  img2.Allocate(w, h, iAlignUp(w, 128), false, NULL, (float*)rimg.data);
  img1.Download();
  img2.Download();

  // Extract Sift features from images
  SiftData siftData1, siftData2;
  float initBlur = 1.0f;
  float thresh = (imgSet ? 4.5f : 3.0f);
  InitSiftData(siftData1, 32768, true, true);
  InitSiftData(siftData2, 32768, true, true);

  // A bit of benchmarking
  //for (int thresh1=1.00f;thresh1<=4.01f;thresh1+=0.50f) {
  float *memoryTmp = AllocSiftTempMemory(w, h, 5, false);
  ExtractSift(siftData1, img1, 5, initBlur, thresh, 0.0f, false, memoryTmp);
  ExtractSift(siftData2, img2, 5, initBlur, thresh, 0.0f, false, memoryTmp);
  FreeSiftTempMemory(memoryTmp);
  std::cout<<siftData1.numPts<<"   "<<siftData2.numPts;
    // Match Sift features and find a homography
  MatchSiftData(siftData1, siftData2);
  
  float homography[9];
  int numMatches;
  FindHomography(siftData1, homography, &numMatches, 10000, 0.00f, 0.80f, 5.0);
  int numFit = ImproveHomography(siftData1, homography, 5, 0.00f, 0.80f, 3.0);

  std::cout << "Number of original features: " <<  siftData1.numPts << " " << siftData2.numPts << std::endl;
  std::cout << "Number of matching features: " << numFit << " " << numMatches << " " << 100.0f*numFit/std::min(siftData1.numPts, siftData2.numPts) << "% " << initBlur << " " << thresh << std::endl;
    //}

  // Print out and store summary data
  PrintMatchData(siftData1, siftData2, img1);
  cv::imwrite("data/limg_pts.jpg", limg);
  cv::imwrite("data/limg_pts.pgm", limg);
  //MatchAll(siftData1, siftData2, homography);

  RetStruct *p = (RetStruct*)malloc(sizeof(struct RetStruct));
  p->numPts = siftData1.numPts;
  p->x_pos =     (float*)malloc(sizeof(float) * p->numPts);
  p->y_pos =     (float*)malloc(sizeof(float) * p->numPts);
  p->m_x_pos =   (float*)malloc(sizeof(float) * p->numPts);
  p->m_y_pos =   (float*)malloc(sizeof(float) * p->numPts);
  p->match_error =     (float*)malloc(sizeof(float) * p->numPts); 
  for(int i=0;i<p->numPts;i++){
	p->x_pos[i] = siftData1.h_data[i].xpos;
	p->y_pos[i] = siftData1.h_data[i].ypos;
	p->m_x_pos[i] = siftData1.h_data[i].match_xpos;
	p->m_y_pos[i] = siftData1.h_data[i].match_ypos;
	p->match_error[i] = siftData1.h_data[i].match_error;
	
  }
  
  // Free Sift data from device
  FreeSiftData(siftData1);
  FreeSiftData(siftData2);
  
  
  
  return p;
}



SiftData* img_sift_process(int height1, int width1, uchar* data1){
  int devNum = 0, imgSet = 0;
  // Read images using OpenCV

  cv::Mat limg(height1, width1, CV_8UC1, data1);
  //cv::flip(limg, rimg, -1);
  unsigned int w = limg.cols;
  unsigned int h = limg.rows;
  std::cout << "Image size = (" << w << "," << h << ")" << std::endl;

  // Initial Cuda images and download images to device
  std::cout << "Initializing data..." << std::endl;
  InitCuda(devNum);
  CudaImage img1;
  img1.Allocate(w, h, iAlignUp(w, 128), false, NULL, (float*)limg.data);
  img1.Download();
  
  // Extract Sift features from images
  SiftData siftData1;
  float initBlur = 1.0f;
  float thresh = (imgSet ? 4.5f : 3.0f);
  InitSiftData(siftData1, 32768, true, true);

  // A bit of benchmarking
  //for (int thresh1=1.00f;thresh1<=4.01f;thresh1+=0.50f) {
  float *memoryTmp = AllocSiftTempMemory(w, h, 5, false); 
  ExtractSift(siftData1, img1, 5, initBlur, thresh, 0.0f, false, memoryTmp);
  FreeSiftTempMemory(memoryTmp);
  SiftData *siftdata_point = &siftData1;
  std::cout<< '1' << siftData1.numPts << ' ' << siftData1.maxPts<< ' ' <<siftData1.d_data <<' ' << siftData1.h_data[0].data[0]<<std::endl;
  
  StructPointerTest *p = (StructPointerTest*)malloc(sizeof(struct StructPointerTest));
  p->x = 101;
  p->y = 201;  
  
  return &siftData1;
}
