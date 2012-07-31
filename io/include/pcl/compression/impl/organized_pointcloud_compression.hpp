/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2009-2012, Willow Garage, Inc.
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Willow Garage, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef ORGANIZED_COMPRESSION_HPP
#define ORGANIZED_COMPRESSION_HPP

#include <pcl/compression/organized_pointcloud_compression.h>

#include <pcl/pcl_macros.h>
#include <pcl/point_cloud.h>

#include <pcl/common/boost.h>
#include <pcl/common/eigen.h>
#include <pcl/common/common.h>
#include <pcl/common/io.h>

#include <pcl/compression/libpng_wrapper.h>
#include <pcl/compression/organized_pointcloud_conversion.h>

#include <string>
#include <vector>
#include <limits>
#include <assert.h>

namespace pcl
{
  namespace io
  {
    //////////////////////////////////////////////////////////////////////////////////////////////
    template<typename PointT> void
    OrganizedPointCloudCompression<PointT>::encodePointCloud (const PointCloudConstPtr &cloud_arg,
                                                              std::ostream& compressedDataOut_arg,
                                                              bool doColorEncoding,
                                                              float depthQuantization_arg,
                                                              int pngLevel_arg,
                                                              bool bShowStatistics_arg)
    {
      uint32_t cloud_width = cloud_arg->width;
      uint32_t cloud_height = cloud_arg->height;

      float maxDepth, vocalLength;

      analyzeOrganizedCloud (cloud_arg, maxDepth, vocalLength);

      // encode header identifier
      compressedDataOut_arg.write (reinterpret_cast<const char*> (frameHeaderIdentifier_), strlen (frameHeaderIdentifier_));
      // encode point cloud width
      compressedDataOut_arg.write (reinterpret_cast<const char*> (&cloud_width), sizeof (cloud_width));
      // encode frame type height
      compressedDataOut_arg.write (reinterpret_cast<const char*> (&cloud_height), sizeof (cloud_height));
      // encode frame max depth
      compressedDataOut_arg.write (reinterpret_cast<const char*> (&maxDepth), sizeof (maxDepth));
      // encode frame focal lenght
      compressedDataOut_arg.write (reinterpret_cast<const char*> (&vocalLength), sizeof (vocalLength));
      // encode frame depth quantization
      compressedDataOut_arg.write (reinterpret_cast<const char*> (&depthQuantization_arg), sizeof (depthQuantization_arg));

      // disparity and rgb image data
      std::vector<uint16_t> disparityData;
      std::vector<uint8_t> rgbData;

      // compressed disparity and rgb image data
      std::vector<uint8_t> compressedDisparity;
      std::vector<uint8_t> compressedRGB;

      uint32_t compressedDisparitySize = 0;
      uint32_t compressedRGBSize = 0;

      // Convert point cloud to disparity and rgb image
      OrganizedConversion<PointT>::convert (*cloud_arg, maxDepth,  depthQuantization_arg, disparityData, rgbData);

      // Compress disparity information
      encodeMonoImageToPNG (disparityData, cloud_width, cloud_height, compressedDisparity, pngLevel_arg);

      compressedDisparitySize = static_cast<uint32_t>(compressedDisparity.size());
      // Encode size of compressed disparity image data
      compressedDataOut_arg.write (reinterpret_cast<const char*> (&compressedDisparitySize), sizeof (compressedDisparitySize));
      // Output compressed disparity to ostream
      compressedDataOut_arg.write (reinterpret_cast<const char*> (&compressedDisparity[0]), compressedDisparity.size () * sizeof(uint8_t));

      // Compress color information
      if (CompressionPointTraits<PointT>::hasColor && doColorEncoding)
        encodeRGBImageToPNG (rgbData, cloud_width, cloud_height, compressedRGB, pngLevel_arg);

      compressedRGBSize = static_cast<uint32_t>(compressedRGB.size ());
      // Encode size of compressed RGB image data
      compressedDataOut_arg.write (reinterpret_cast<const char*> (&compressedRGBSize), sizeof (compressedRGBSize));
      // Output compressed disparity to ostream
      compressedDataOut_arg.write (reinterpret_cast<const char*> (&compressedRGB[0]), compressedRGB.size () * sizeof(uint8_t));

      /*
       // PNG decoding
       size_t png_width;
       size_t png_height;
       unsigned int png_channels;
       unsigned int png_bitDepth;
       std::vector<uint16_t> imageDataRec;
       decodePNGImage(compressedImage, imageDataRec, png_width, png_height, png_channels, png_bitDepth);

       size_t i;
       for (i=0; i<imageData.size(); ++i)
       {
       assert (imageData[i]==imageDataRec[i]);
       }
       assert (imageData.size()==imageDataRec.size());

       pcl::PointCloud<PointT> cloudTest;

       // reconstruct point cloud
       OrganizedConversion<PointT>::convert(imageDataRec, cloud_width, cloud_height, maxDepth, vocalLength, cloudTest);

       for (i=0; i<cloudTest.points.size(); ++i)
       {
       if (pcl::isFinite(cloud_arg->points[i]))
       assert (cloud_arg->points[i].rgb==cloudTest.points[i].rgb);
       }
       assert (cloud_arg->points.size()==cloudTest.points.size());
       */

      if (bShowStatistics_arg)
      {
        uint64_t pointCount = cloud_width * cloud_height;
        float bytesPerPoint = static_cast<float> (compressedDisparitySize+compressedRGBSize) / static_cast<float> (pointCount);

        PCL_INFO("*** POINTCLOUD ENCODING ***\n");
        PCL_INFO("Number of encoded points: %ld\n", pointCount);
        PCL_INFO("Size of uncompressed point cloud: %.2f kBytes\n", (static_cast<float> (pointCount) * CompressionPointTraits<PointT>::bytesPerPoint) / 1024.0f);
        PCL_INFO("Size of compressed point cloud: %.2f kBytes\n", static_cast<float> (compressedDisparitySize+compressedRGBSize) / 1024.0f);
        PCL_INFO("Total bytes per point: %.4f bytes\n", static_cast<float> (bytesPerPoint));
        PCL_INFO("Total compression percentage: %.4f%%\n", (bytesPerPoint) / (CompressionPointTraits<PointT>::bytesPerPoint) * 100.0f);
        PCL_INFO("Compression ratio: %.2f\n\n", static_cast<float> (CompressionPointTraits<PointT>::bytesPerPoint) / bytesPerPoint);
      }
    }

    //////////////////////////////////////////////////////////////////////////////////////////////
    template<typename PointT> void
    OrganizedPointCloudCompression<PointT>::decodePointCloud (std::istream& compressedDataIn_arg,
                                                              PointCloudPtr &cloud_arg,
                                                              bool bShowStatistics_arg)
    {
      // sync to frame header
      unsigned int headerIdPos = 0;
      while (headerIdPos < strlen (frameHeaderIdentifier_))
      {
        char readChar;
        compressedDataIn_arg.read (static_cast<char*> (&readChar), sizeof (readChar));
        if (readChar != frameHeaderIdentifier_[headerIdPos++])
          headerIdPos = (frameHeaderIdentifier_[0] == readChar) ? 1 : 0;
      }

      uint32_t cloud_width;
      uint32_t cloud_height;
      float maxDepth, vocalLength;
      float depthQuantization;

      // reading frame header
      compressedDataIn_arg.read (reinterpret_cast<char*> (&cloud_width), sizeof (cloud_width));
      compressedDataIn_arg.read (reinterpret_cast<char*> (&cloud_height), sizeof (cloud_height));
      compressedDataIn_arg.read (reinterpret_cast<char*> (&maxDepth), sizeof (maxDepth));
      compressedDataIn_arg.read (reinterpret_cast<char*> (&vocalLength), sizeof (vocalLength));
      compressedDataIn_arg.read (reinterpret_cast<char*> (&depthQuantization), sizeof (depthQuantization));

      // disparity and rgb image data
      std::vector<uint16_t> disparityData;
      std::vector<uint8_t> rgbData;

      // compressed disparity and rgb image data
      std::vector<uint8_t> compressedDisparity;
      std::vector<uint8_t> compressedRGB;

      uint32_t compressedDisparitySize;
      uint32_t compressedRGBSize;

      // reading compressed disparity data
      compressedDataIn_arg.read (reinterpret_cast<char*> (&compressedDisparitySize), sizeof (compressedDisparitySize));
      compressedDisparity.resize (compressedDisparitySize);
      compressedDataIn_arg.read (reinterpret_cast<char*> (&compressedDisparity[0]), compressedDisparitySize * sizeof(uint8_t));

      // reading compressed rgb data
      compressedDataIn_arg.read (reinterpret_cast<char*> (&compressedRGBSize), sizeof (compressedRGBSize));
      compressedRGB.resize (compressedRGBSize);
      compressedDataIn_arg.read (reinterpret_cast<char*> (&compressedRGB[0]), compressedRGBSize * sizeof(uint8_t));

      // PNG decoded parameters
      size_t png_width;
      size_t png_height;
      unsigned int png_channels;

      // decode PNG compressed disparity data
      decodePNGToImage (compressedDisparity, disparityData, png_width, png_height, png_channels);

      // decode PNG compressed rgb data
      decodePNGToImage (compressedRGB, rgbData, png_width, png_height, png_channels);

      // reconstruct point cloud
      OrganizedConversion<PointT>::convert (disparityData, rgbData, cloud_width, cloud_height, maxDepth, depthQuantization, vocalLength, *cloud_arg);

      if (bShowStatistics_arg)
      {
        uint64_t pointCount = cloud_width * cloud_height;
        float bytesPerPoint = static_cast<float> (compressedDisparitySize+compressedRGBSize) / static_cast<float> (pointCount);

        PCL_INFO("*** POINTCLOUD DECODING ***\n");
        PCL_INFO("Number of encoded points: %ld\n", pointCount);
        PCL_INFO("Size of uncompressed point cloud: %.2f kBytes\n", (static_cast<float> (pointCount) * CompressionPointTraits<PointT>::bytesPerPoint) / 1024.0f);
        PCL_INFO("Size of compressed point cloud: %.2f kBytes\n", static_cast<float> (compressedDisparitySize+compressedRGBSize) / 1024.0f);
        PCL_INFO("Total bytes per point: %.4f bytes\n", static_cast<float> (bytesPerPoint));
        PCL_INFO("Total compression percentage: %.4f%%\n", (bytesPerPoint) / (CompressionPointTraits<PointT>::bytesPerPoint) * 100.0f);
        PCL_INFO("Compression ratio: %.2f\n\n", static_cast<float> (CompressionPointTraits<PointT>::bytesPerPoint) / bytesPerPoint);
      }
    }

    //////////////////////////////////////////////////////////////////////////////////////////////
    template<typename PointT> void
    OrganizedPointCloudCompression<PointT>::analyzeOrganizedCloud (PointCloudConstPtr cloud_arg,
                                                                   float& maxDepth_arg,
                                                                   float& vocalLength_arg) const
    {
      size_t width, height, it;
      int centerX, centerY;
      int x, y;
      float maxDepth;
      float focalLength;

      width = cloud_arg->width;
      height = cloud_arg->height;

      // Center of organized point cloud
      centerX = static_cast<int> (width / 2);
      centerY = static_cast<int> (height / 2);

      // Ensure we have an organized point cloud
      assert((width>1) && (height>1));
      assert(width*height == cloud_arg->points.size());

      maxDepth = 0;
      focalLength = 0;

      it = 0;
      for (y = -centerY; y < +centerY; ++y)
        for (x = -centerX; x < +centerX; ++x)
        {
          const PointT& point = cloud_arg->points[it++];

          if (pcl::isFinite (point))
          {
            if (maxDepth < point.z)
            {
              // Update maximum depth
              maxDepth = point.z;

              // Calculate focal length
              focalLength = 2.0f / (point.x / (static_cast<float> (x) * point.z) + point.y / (static_cast<float> (y) * point.z));
            }
          }
        }

      // Update return values
      maxDepth_arg = maxDepth;
      vocalLength_arg = focalLength;
    }

  }
}

#endif
