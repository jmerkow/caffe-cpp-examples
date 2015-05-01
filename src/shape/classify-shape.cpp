// The MIT License (MIT)
//
// Copyright (c) 2015 Boaz Stolk
//
// For full license view project root directory

// This program classifies a random generated image using a network and a trained model
// Usage:
//  classify-shape [FLAGS] NET MODEL
//

#include <boost/scoped_ptr.hpp>
#include <gflags/gflags.h>
#include <caffe/util/db.hpp>
#include <caffe/caffe.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <string>
#include <vector>
#include <algorithm>

int
main(int argc, char* argv[])
{
  ::google::InitGoogleLogging(argv[0]);

#ifndef GFLAGS_GFLAGS_H_
  namespace gflags = google;
#endif

  gflags::SetUsageMessage("Classifies a random generated image using a network and a trained model\n"
                          "Usage:\n"
                          " classify-shape NET MODEL\n");
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  if (argc != 3)
  {
    gflags::ShowUsageWithFlagsRestrict(argv[0], "classify-shape");
    return 1;
  }

  // get the net
  const std::string sNetwork = argv[1];
  std::cout << "loading " << sNetwork << std::endl;
  caffe::Net<float> caffe_test_net(sNetwork, caffe::TEST);
  caffe_test_net.set_debug_info(true);

  // get trained model
  const std::string sModel = argv[2];
  std::cout << "loading " << sModel << std::endl;
  caffe_test_net.CopyTrainedLayersFrom(sModel);

  // seed random generator
  std::srand(std::time(NULL));

  // generate random input
  typedef float tDataType;
  typedef int tLabelType;

  { // create input
    typedef std::vector<tDataType> tInOutVec;
    typedef std::pair<tInOutVec, tLabelType> tPair;
    typedef std::vector<tPair> tPairs;
    tPairs pairs;
    // generate image
    const int rows = 200;
    const int cols = 200;
    cv::Mat in_image_bgr = cv::Mat::zeros(rows, cols, CV_8UC3);

    // rectangle (red)
    for (int i = 0; i < 3; i++)
    {
      const int x1 = std::min(std::max(((float)std::rand() / RAND_MAX) * in_image_bgr.cols, (float)20), (float)rows-20);
      const int y1 = std::min(std::max(((float)std::rand() / RAND_MAX) * in_image_bgr.rows, (float)20), (float)cols-20);
      const int x2 = x1 + 8;
      const int y2 = y1 + 8;

      cv::rectangle(in_image_bgr,
        cv::Point(x1, y1),
        cv::Point(x2, y2),
        cv::Scalar(0, 0, 255),
        -1,
        8);
    }

    // circle (green)
    for (int i = 0; i < 3; i++)
    {
      const int x1 = std::min(std::max(((float)std::rand() / RAND_MAX) * in_image_bgr.cols, (float)20), (float)rows-20);
      const int y1 = std::min(std::max(((float)std::rand() / RAND_MAX) * in_image_bgr.rows, (float)20), (float)cols-20);
      cv::circle(in_image_bgr,
        cv::Point(x1, y1),
        5,
        cv::Scalar(0, 255, 0),
        -1,
        8);
    }

    // binarize for classification
    cv::Mat in_image_gray;
    cv::cvtColor(in_image_bgr, in_image_gray, CV_RGB2GRAY);
    cv::Mat in_image = in_image_gray > 0;

    // show input
    cv::namedWindow("in", CV_WINDOW_AUTOSIZE);
    cv::moveWindow("in", 20, 20);
    cv::imshow("in", in_image);

    // create output
    cv::Mat out_image_bgr = cv::Mat::zeros(in_image.size(), CV_8UC3);

    // create output
    long iProcessedPixels = 0, iCorrectPixels = 0;
    const int kernel = 15;
    const int h_kernel = kernel / 2;
    for (int y = h_kernel; y < in_image.rows - h_kernel - 1; y++)
    {
      // create input blob for a whole line which is much faster (instead of a classification per pixel)
      caffe::BlobProto blob_proto;
      caffe::BlobShape* shape = blob_proto.mutable_shape();
      shape->add_dim(cols - 2 * h_kernel);
      shape->add_dim(kernel * kernel);

      for (int x = h_kernel; x < in_image.cols - h_kernel - 1; x++)
      {
        iProcessedPixels++;
        const cv::Vec3b vec = in_image.at<cv::Vec3b>(y, x);
        for (int yk = y - h_kernel; yk < y + h_kernel + 1; yk++)
        {
          for (int xk = x - h_kernel; xk < x + h_kernel + 1; xk++)
          {
            const uchar val = in_image.at<uchar>(yk, xk);
            blob_proto.add_data(val / 255.0f);
          }
        }
      }

      std::vector<int> vShape;
      vShape.push_back(cols - 2 * h_kernel);
      vShape.push_back(kernel * kernel);
      caffe::Blob<float> blob(vShape);

      // set data into blob
      blob.FromProto(blob_proto);

      // fill the bottom vector
      std::vector<caffe::Blob<float>*> bottom;
      bottom.push_back(&blob);

      // forward pass
      float loss = 0.0;
      const std::vector<caffe::Blob<float>*>& result = caffe_test_net.Forward(bottom, &loss);

      // mark classification result in output image
      for (int x = h_kernel, batch = 0; x < in_image.cols - h_kernel - 1; x++, batch++)
      {
        // find maximum
        float max = -1;
        int max_i = -1;
        for (int i = 0; i < 3; ++i)
        {
          const float value = (result[0]->cpu_data())[i + batch * 3];
          if (value > max)
          {
            max = value;
            max_i = i;
          }
        }

        switch (max_i)
        {
          case 0: // class 0: background
            out_image_bgr.at<cv::Vec3b>(y, x) = cv::Vec3b(0, 0, 0);
            if (out_image_bgr.at<cv::Vec3b>(y, x) == in_image_bgr.at<cv::Vec3b>(y, x))
              iCorrectPixels++;
            break;
          case 1: // class 1: circle
            out_image_bgr.at<cv::Vec3b>(y, x) = cv::Vec3b(0, 255, 0);
            if (out_image_bgr.at<cv::Vec3b>(y, x) == in_image_bgr.at<cv::Vec3b>(y, x))
              iCorrectPixels++;
            break;
          case 2: // class 2: square
            out_image_bgr.at<cv::Vec3b>(y, x) = cv::Vec3b(0, 0, 255);
            if (out_image_bgr.at<cv::Vec3b>(y, x) == in_image_bgr.at<cv::Vec3b>(y, x))
              iCorrectPixels++;
            break;
          default:
            break;
        }
      }
    }

    std::cout << "classified " << static_cast<double>(iCorrectPixels) / iProcessedPixels << "% correctly" << std::endl;

    cv::namedWindow("result", CV_WINDOW_AUTOSIZE);
    cv::moveWindow("result", 240, 20);
    cv::imshow("result", out_image_bgr);
    cv::waitKey(0);
  }

  return 0;
}