// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rga.h"

#include <assert.h>

#include <vector>

#include <rga/RockchipRga.h>

#include "buffer.h"
#include "filter.h"

namespace easymedia {

class RgaFilter : public Filter {
public:
  RgaFilter(const char *param);
  virtual ~RgaFilter() = default;
  static const char *GetFilterName() { return "rkrga"; }
  virtual int Process(std::shared_ptr<MediaBuffer> input,
                      std::shared_ptr<MediaBuffer> output) override;

  static RockchipRga gRkRga;

private:
  std::vector<ImageRect> vec_rect;
  int rotate;
};

RockchipRga RgaFilter::gRkRga;

RgaFilter::RgaFilter(const char *param) : rotate(0) {
  std::map<std::string, std::string> params;
  if (!parse_media_param_map(param, params)) {
    SetError(-EINVAL);
    return;
  }
  const std::string &value = params[KEY_BUFFER_RECT];
  auto &&rects = StringToTwoImageRect(value);
  if (rects.empty()) {
    LOG("missing rects\n");
    SetError(-EINVAL);
    return;
  }
  vec_rect = std::move(rects);
  const std::string &v = params[KEY_BUFFER_ROTATE];
  if (!v.empty())
    rotate = std::stoi(v);
}

int RgaFilter::Process(std::shared_ptr<MediaBuffer> input,
                       std::shared_ptr<MediaBuffer> output) {
  if (vec_rect.size() < 2)
    return -EINVAL;
  if (!input || input->GetType() != Type::Image)
    return -EINVAL;
  if (!output || output->GetType() != Type::Image)
    return -EINVAL;

  auto src = std::static_pointer_cast<easymedia::ImageBuffer>(input);
  ImageRect *src_rect = nullptr;
  if (vec_rect[0].w > 0 && vec_rect[0].h > 0)
    src_rect = &vec_rect[0];
  auto dst = std::static_pointer_cast<easymedia::ImageBuffer>(output);
  ImageRect *dst_rect = nullptr;
  if (vec_rect[1].w > 0 && vec_rect[1].h > 0)
    dst_rect = &vec_rect[1];
  assert(!dst_rect || (dst_rect && dst->IsValid()));
  if (!dst_rect && !dst->IsValid()) {
    // the same to src
    ImageInfo info = src->GetImageInfo();
    info.pix_fmt = dst->GetPixelFormat();
    size_t size = CalPixFmtSize(info);
    if (size == 0)
      return -EINVAL;
    auto &&mb = MediaBuffer::Alloc2(size, MediaBuffer::MemType::MEM_HARD_WARE);
    ImageBuffer ib(mb, info);
    if (ib.GetSize() >= size) {
      ib.SetValidSize(size);
      *dst.get() = ib;
    }
    assert(dst->IsValid());
  }
  return rga_blit(src, dst, src_rect, dst_rect, rotate);
}

static int get_rga_format(PixelFormat f) {
  static std::map<PixelFormat, int> rga_format_map = {
      {PIX_FMT_YUV420P, RK_FORMAT_YCrCb_420_P},
      {PIX_FMT_NV12, RK_FORMAT_YCrCb_420_SP},
      {PIX_FMT_NV21, RK_FORMAT_YCbCr_420_SP},
      {PIX_FMT_YUV422P, RK_FORMAT_YCrCb_422_P},
      {PIX_FMT_NV16, RK_FORMAT_YCrCb_422_SP},
      {PIX_FMT_NV61, RK_FORMAT_YCbCr_422_SP},
      {PIX_FMT_YUYV422, -1},
      {PIX_FMT_UYVY422, -1},
      {PIX_FMT_RGB565, RK_FORMAT_RGB_565},
      {PIX_FMT_BGR565, -1},
      {PIX_FMT_RGB888, RK_FORMAT_RGB_888},
      {PIX_FMT_BGR888, RK_FORMAT_BGR_888},
      {PIX_FMT_ARGB8888, RK_FORMAT_RGBA_8888},
      {PIX_FMT_ABGR8888, RK_FORMAT_BGRA_8888}};
  auto it = rga_format_map.find(f);
  if (it != rga_format_map.end())
    return it->second;
  return -1;
}

#ifndef NDEBUG
static void dummp_rga_info(rga_info_t info, std::string name) {
  LOGD("\n### %s dummp info:\n", name.c_str());
  LOGD("\t info.fd = %d\n", info.fd);
  LOGD("\t info.mmuFlag = %d\n", info.mmuFlag);
  LOGD("\t info.rotation = %d\n", info.rotation);
  LOGD("\t info.blend = %d\n", info.blend);
  LOGD("\t info.virAddr = %p\n", info.virAddr);
  LOGD("\t info.phyAddr = %p\n", info.phyAddr);
  LOGD("\t info.rect.xoffset = %d\n", info.rect.xoffset);
  LOGD("\t info.rect.yoffset = %d\n", info.rect.yoffset);
  LOGD("\t info.rect.width = %d\n", info.rect.width);
  LOGD("\t info.rect.height = %d\n", info.rect.height);
  LOGD("\t info.rect.wstride = %d\n", info.rect.wstride);
  LOGD("\t info.rect.hstride = %d\n", info.rect.hstride);
  LOGD("\t info.rect.format = %d\n", info.rect.format);
  LOGD("\t info.rect.size = %d\n", info.rect.size);
  LOGD("\n");
}
#endif

int rga_blit(std::shared_ptr<ImageBuffer> src, std::shared_ptr<ImageBuffer> dst,
             ImageRect *src_rect, ImageRect *dst_rect, int rotate) {
  if (!src || !src->IsValid())
    return -EINVAL;
  if (!dst || !dst->IsValid())
    return -EINVAL;
  rga_info_t src_info, dst_info;
  memset(&src_info, 0, sizeof(src_info));
  src_info.fd = src->GetFD();
  if (src_info.fd < 0)
    src_info.virAddr = src->GetPtr();
  src_info.mmuFlag = 1;
  src_info.rotation = rotate;
  if (src_rect)
    rga_set_rect(&src_info.rect, src_rect->x, src_rect->y, src_rect->w,
                 src_rect->h, src->GetVirWidth(), src->GetVirHeight(),
                 get_rga_format(src->GetPixelFormat()));
  else
    rga_set_rect(&src_info.rect, 0, 0, src->GetWidth(), src->GetHeight(),
                 src->GetVirWidth(), src->GetVirHeight(),
                 get_rga_format(src->GetPixelFormat()));

  memset(&dst_info, 0, sizeof(dst_info));
  dst_info.fd = dst->GetFD();
  if (dst_info.fd < 0)
    dst_info.virAddr = dst->GetPtr();
  dst_info.mmuFlag = 1;
  if (dst_rect)
    rga_set_rect(&dst_info.rect, dst_rect->x, dst_rect->y, dst_rect->w,
                 dst_rect->h, dst->GetVirWidth(), dst->GetVirHeight(),
                 get_rga_format(dst->GetPixelFormat()));
  else
    rga_set_rect(&dst_info.rect, 0, 0, dst->GetWidth(), dst->GetHeight(),
                 dst->GetVirWidth(), dst->GetVirHeight(),
                 get_rga_format(dst->GetPixelFormat()));

#ifndef NDEBUG
  dummp_rga_info(src_info, "SrcInfo");
  dummp_rga_info(dst_info, "DstInfo");
#endif

  int ret = RgaFilter::gRkRga.RkRgaBlit(&src_info, &dst_info, NULL);
  if (ret) {
    dst->SetValidSize(0);
    LOG("Fail to RkRgaBlit, ret=%d\n", ret);
  } else {
    size_t valid_size = CalPixFmtSize(dst->GetPixelFormat(),
      dst->GetVirWidth(), dst->GetVirHeight(), 0);
    dst->SetValidSize(valid_size);
    if (src->GetUSTimeStamp() > dst->GetUSTimeStamp())
      dst->SetUSTimeStamp(src->GetUSTimeStamp());
  }
  return ret;
}

class _PRIVATE_SUPPORT_FMTS : public SupportMediaTypes {
public:
  _PRIVATE_SUPPORT_FMTS() {
    types.append(TYPENEAR(IMAGE_YUV420P));
    types.append(TYPENEAR(IMAGE_NV12));
    types.append(TYPENEAR(IMAGE_NV21));
    types.append(TYPENEAR(IMAGE_YUV422P));
    types.append(TYPENEAR(IMAGE_NV16));
    types.append(TYPENEAR(IMAGE_NV61));
    // types.append(TYPENEAR(IMAGE_YUYV422));
    // types.append(TYPENEAR(IMAGE_UYVY422));
    types.append(TYPENEAR(IMAGE_RGB565));
    types.append(TYPENEAR(IMAGE_BGR565));
    types.append(TYPENEAR(IMAGE_RGB888));
    types.append(TYPENEAR(IMAGE_BGR888));
    types.append(TYPENEAR(IMAGE_ARGB8888));
    types.append(TYPENEAR(IMAGE_ABGR8888));
  }
};
static _PRIVATE_SUPPORT_FMTS priv_fmts;

DEFINE_COMMON_FILTER_FACTORY(RgaFilter)
const char *FACTORY(RgaFilter)::ExpectedInputDataType() {
  return priv_fmts.types.c_str();
}
const char *FACTORY(RgaFilter)::OutPutDataType() {
  return priv_fmts.types.c_str();
}

} // namespace easymedia
