#include "easypr/core/plate_recognize.h"
#include "easypr/config.h"
#include "thirdparty/textDetect/erfilter.hpp"

namespace easypr {

CPlateRecognize::CPlateRecognize() { 
  m_showResult = false;
}

void groups_draw(Mat &src, std::vector<Rect> &groups) {
  for (int i = (int)groups.size() - 1; i >= 0; i--)
  {
    if (src.type() == CV_8UC3)
      rectangle(src, groups.at(i).tl(), groups.at(i).br(), Scalar(0, 255, 255), 3, 8);
    else
      rectangle(src, groups.at(i).tl(), groups.at(i).br(), Scalar(255), 3, 8);
  }
}

void er_show(std::vector<Mat> &channels, std::vector<std::vector<text::ERStat> > &regions)
{
  for (int c = 0; c<(int)channels.size(); c++)
  {
    Mat dst = Mat::zeros(channels[0].rows + 2, channels[0].cols + 2, CV_8UC1);
    for (int r = 0; r<(int)regions[c].size(); r++)
    {
      text::ERStat er = regions[c][r];
      if (er.parent != NULL) // deprecate the root region
      {
        int newMaskVal = 255;
        int flags = 4 + (newMaskVal << 8) + FLOODFILL_FIXED_RANGE + FLOODFILL_MASK_ONLY;
        floodFill(channels[c], dst, Point(er.pixel%channels[c].cols, er.pixel / channels[c].cols),
                  Scalar(255), 0, Scalar(er.level), Scalar(0), flags);
      }
    }
    char buff[10]; char *buff_ptr = buff;
    sprintf(buff, "channel %d", c);
    imshow(buff_ptr, dst);
  }
  waitKey(-1);
}

int CPlateRecognize::plateRecognizeAsTextNM(Mat src, std::vector<CPlate> &licenseVec) {
  std::vector<Mat> channels;
  std::vector<int> flags;

  if (1) {
    Mat grayImage;
    cvtColor(src, grayImage, COLOR_BGR2GRAY);
    channels.push_back(grayImage);

    //Mat singleChannelImage;
    //extractChannel(src, singleChannelImage, 0);
    //channels.push_back(singleChannelImage);

    flags.push_back(0);
  }

  //text::computeNMChannels(src, channels);

  int cn = (int)channels.size();
  // Append negative channels to detect ER- (bright regions over dark background)
  for (int c = 0; c < cn; c++)
    channels.push_back(255 - channels[c]);

  // Create ERFilter objects with the 1st and 2nd stage default classifiers
  std::string pathMMmodel_1 = "resources/model/text/trained_classifierNM1.xml";
  std::string pathMMmodel_2 = "resources/model/text/trained_classifierNM2.xml";

  Ptr<text::ERFilter> er_filter1 = text::createERFilterNM1(text::loadClassifierNM1(pathMMmodel_1), 16, 0.00015f, 0.13f, 0.2f, true, 0.1f);
  Ptr<text::ERFilter> er_filter2 = text::createERFilterNM2(text::loadClassifierNM2(pathMMmodel_2), 0.5);

  std::vector<std::vector<text::ERStat> > regions(channels.size());
  // Apply the default cascade classifier to each independent channel (could be done in parallel)
  std::cout << "Extracting Class Specific Extremal Regions from " << (int)channels.size() << " channels ..." << std::endl;
  std::cout << "    (...) this may take a while (...)" << std::endl << std::endl;
  for (int c = 1; c < (int)channels.size(); c++)
  {
    er_filter1->run(channels[c], regions[c]);
    //er_filter2->run(channels[c], regions[c]);
  }

  // Detect character groups
  std::cout << "Grouping extracted ERs ... ";
  std::vector< std::vector<Vec2i> > region_groups;
  std::vector<Rect> groups_boxes;

  //er_show(channels, regions);

  //erGrouping(src, channels, regions, region_groups, groups_boxes, text::ERGROUPING_ORIENTATION_HORIZ);
  //erGrouping(src, channels, regions, region_groups, groups_boxes, ERGROUPING_ORIENTATION_ANY, "trained_classifier_erGrouping.xml", 0.5);

  //groups_draw(src, groups_boxes);
  //imshow("grouping", src);
  //waitKey(0);

  er_show(channels, regions);

  // draw groups

  // memory clean-up
  er_filter1.release();
  er_filter2.release();
  regions.clear();
  if (!groups_boxes.empty())
  {
    groups_boxes.clear();
  }

  return 0;
}

int CPlateRecognize::plateRecognizeAsText(Mat src, std::vector<CPlate> &licenseVec)
{
  //TODO
  return 0;
}


// !车牌识别模块
int CPlateRecognize::plateRecognize(Mat src, std::vector<CPlate> &licenseVec, int img_index) {

  // 车牌集合
  std::vector<CPlate> plateVec;

  // 对图像进行统一缩放，确保图像不要过大
  // 对速度提升帮助不大

  //int scale_size = 1600;
  //double scale_ratio = 1;
  //Mat ret = scaleImage(src, Size(scale_size, scale_size), scale_ratio);
  //src = ret;

  // 进行深度定位，使用颜色信息与二次Sobel

  int resultPD = plateDetect(src, plateVec, img_index);

  if (resultPD == 0) {
    size_t num = plateVec.size();
    int index = 0;

    //依次识别每个车牌内的符号
    for (size_t j = 0; j < num; j++) {
      CPlate item = plateVec.at(j);
      Mat plateMat = item.getPlateMat();

      if (0) {
        imshow("plate", plateMat);
        waitKey(0);
        destroyWindow("plate");
      }
     
      Color color = item.getPlateColor();
      if (color == UNKNOWN) {
        color = getPlateType(plateMat, true);
        item.setPlateColor(color);
      }

      std::string plateColor = getPlateColor(color);
      if (1) {
        std::cout << "plateColor:" << plateColor << std::endl;
      }

      //获取车牌号
      std::string plateIdentify = "";
      int resultCR = charsRecognise(item, plateIdentify);
           
      if (resultCR == 0) {
        std::string license = plateColor + ":" + plateIdentify;
        item.setPlateStr(license);
        licenseVec.push_back(item);
      }
      else {
        std::string license = plateColor;
        item.setPlateStr(license);
        licenseVec.push_back(item);
        if (1) {
          std::cout << "resultCR:" << resultCR << std::endl;
        }
      }
    }

    //完整识别过程到此结束

    //如果是Debug模式，则还需要将定位的图片显示在原图左上角

    if (getResultShow()) {
      Mat result;
      src.copyTo(result);

      for (size_t j = 0; j < num; j++) {
        CPlate item = plateVec[j];
        Mat plateMat = item.getPlateMat();

        int height = 36;
        int width = 136;
        if (height * index + height < result.rows) {
          Mat imageRoi = result(Rect(0, 0 + height * index, width, height));
          addWeighted(imageRoi, 0, plateMat, 1, 0, imageRoi);
        }
        index++;

        RotatedRect minRect = item.getPlatePos();
        Point2f rect_points[4];
        minRect.points(rect_points);

        Scalar lineColor = Scalar(255, 255, 255);

        if (item.getPlateLocateType() == SOBEL) lineColor = Scalar(255, 0, 0);
        if (item.getPlateLocateType() == COLOR) lineColor = Scalar(0, 255, 0);
        if (item.getPlateLocateType() == CMSER) lineColor = Scalar(0, 0, 255);

        for (int j = 0; j < 4; j++)
          line(result, rect_points[j], rect_points[(j + 1) % 4], lineColor, 2, 8);
      }

      showResult(result);
    }
  }
  return resultPD;
}


int CPlateRecognize::plateRecognize(Mat src,
                                    std::vector<std::string> &licenseVec) {

  // 车牌方块集合

  std::vector<CPlate> plateVec;

  // 进行深度定位，使用颜色信息与二次Sobel

  int resultPD = plateDetect(src, plateVec, 0, kDebug, 0);

  if (resultPD == 0) {
    size_t num = plateVec.size();
    int index = 0;

    //依次识别每个车牌内的符号

    for (size_t j = 0; j < num; j++) {
      CPlate item = plateVec[j];
      Mat plate = item.getPlateMat();

      //获取车牌颜色

      std::string plateType = getPlateColor(plate);

      //获取车牌号

      std::string plateIdentify = "";
      int resultCR = charsRecognise(plate, plateIdentify);
      if (resultCR == 0) {
        std::string license = plateType + ":" + plateIdentify;
        licenseVec.push_back(license);
      }
    }

    //完整识别过程到此结束

    //如果是Debug模式，则还需要将定位的图片显示在原图左上角

    if (getResultShow()) {
      Mat result;
      src.copyTo(result);

      for (size_t j = 0; j < num; j++) {
        CPlate item = plateVec[j];
        Mat plate = item.getPlateMat();

        int height = 36;
        int width = 136;
        if (height * index + height < result.rows) {
          Mat imageRoi = result(Rect(0, 0 + height * index, width, height));
          addWeighted(imageRoi, 0, plate, 1, 0, imageRoi);
        }
        index++;

        RotatedRect minRect = item.getPlatePos();
        Point2f rect_points[4];
        minRect.points(rect_points);

        Scalar lineColor = Scalar(255, 255, 255);

        if (item.getPlateLocateType() == SOBEL) lineColor = Scalar(255, 0, 0);

        if (item.getPlateLocateType() == COLOR) lineColor = Scalar(0, 255, 0);

        for (int j = 0; j < 4; j++)
          line(result, rect_points[j], rect_points[(j + 1) % 4], lineColor, 2,
               8);
      }

      //显示定位框的图片

      showResult(result);
    }
  }

  return resultPD;
}
}