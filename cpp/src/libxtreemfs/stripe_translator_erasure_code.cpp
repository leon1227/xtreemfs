/*
 * Copyright (c) 2009-2011 by Tanja Baranova, Zuse Institute Berlin
 *
 * Licensed under the BSD License, see LICENSE file for details.
 *
 */
#include "libxtreemfs/stripe_translator_erasure_code.h"
#include "libxtreemfs/xtreemfs_exception.h"

#include <algorithm>
#include <boost/dynamic_bitset.hpp>
#include <vector>

using namespace std;
using namespace xtreemfs::pbrpc;

namespace xtreemfs {

void StripeTranslatorErasureCode::TranslateWriteRequest(
    const char *buf,
    size_t size,
    int64_t offset,
    PolicyContainer policies,
    std::vector<WriteOperation>* operations) const {
  // stripe size is stored in kB
  unsigned int stripe_size = (*policies.begin())->stripe_size() * 1024;
  // number of OSD to distribute parity chunks
  unsigned int parity_width =(*policies.begin())->parity_width();
  // number of OSD to distribute chunks
  unsigned int width =(*policies.begin())->width();
  unsigned int data_width = width - parity_width;
  char *buf_redundance = NULL;

  size_t obj_offset = offset / stripe_size;

  // check for illegal offset
  assert(offset % (stripe_size * data_width) == 0);

  // object number for stripe (parity stripes are not counted)
  size_t obj_number = obj_offset;
  // actual object number to calculate the osd number
  size_t real_obj_number =  (obj_offset / data_width * width);

  size_t start = 0;

  while (start < size) {

    std::vector<size_t> osd_offsets;

    size_t osd_number = real_obj_number % width;
    osd_offsets.push_back(osd_number);

    size_t req_size
      = min(size - start, static_cast<size_t>(stripe_size));
    //is it a parity chunk?
    if (osd_number >= data_width) {
      buf_redundance = new char[stripe_size];

      for (size_t i = 0; i < stripe_size; ++i) {
        for (size_t j = width; j > 1; --j)
        buf_redundance[i] = buf[start - (j * stripe_size) + i]
            ^ buf[start - ((j-1) * stripe_size) + i];
      }

      /* use the last object number used for a data stripe
       * this caters towards the calculation of a correct  filesize
       * ...and hopefully nothing else */
      operations->push_back(
          WriteOperation(obj_number - data_width, osd_offsets, stripe_size, 0,
              buf_redundance, true));
    } else {
      operations->push_back(
          WriteOperation(obj_number, osd_offsets, req_size, 0,
              buf + start));

      start += req_size;
      obj_number++;
    }
    real_obj_number++;

    // all data has been written...write last parity block
    if (size == start) {
      osd_offsets.clear();
      osd_offsets.push_back(data_width);
      if (obj_number % data_width == 1){
        // partial object at the start of the stripe
        // write data to parity object and enque par object with correct osd_offsets
        operations->push_back(
            WriteOperation(obj_number - 1, osd_offsets, req_size, 0,
              buf + start - req_size));
      } else {
        buf_redundance = new char[stripe_size];
        size_t data_index = operations->size() - 1;
        memcpy(buf_redundance, (*operations)[data_index].data, req_size);
        memset(buf_redundance + req_size, 0, stripe_size - req_size);

        // XOR all obects for the length of the incomplete object
        // for (size_t j = obj_number % data_width; j > 0; --j){
        data_index--;
          for (size_t i = 0; i < stripe_size; ++i) {
            buf_redundance[i] = (*operations)[data_index].data[i]
              ^ buf_redundance[i];
          }
        // }
        --obj_number;
        operations->push_back(
            WriteOperation(obj_number - (obj_number % data_width), osd_offsets, stripe_size, 0,
              buf_redundance, true));
        // full objects have been written in this line
        // create parity object and enque at correct position..remember N+1 striping
      }
      return;
    }
  }
}

size_t StripeTranslatorErasureCode::TranslateReadRequest(
    char *buf,
    size_t size,
    int64_t offset,
    PolicyContainer policies,
    std::vector<ReadOperation>* operations) const {
  // stripe size is stored in kB
  unsigned int stripe_size = (*policies.begin())->stripe_size() * 1024;
  // number of OSD to distribute parity chunks
  unsigned int parity_width =(*policies.begin())->parity_width();
  // number of OSD to distribute chunks
  unsigned int width =(*policies.begin())->width();
  unsigned int data_width = width - parity_width;
  char *buf_redundance = NULL;

  size_t obj_offset = offset / stripe_size;
  offset = offset % stripe_size;

  // object number for stripe (parity stripes are not counted)
  size_t obj_number = obj_offset - (obj_offset % data_width);
  // actual object number to calculate the osd number
  size_t real_obj_number =  (obj_offset / data_width * width);
  if (real_obj_number != 0)
    real_obj_number -= (obj_offset % data_width);
  size_t data_reads = 0;

  size_t start = 0;

  for (int i = 0; i < obj_offset % data_width; i--) {

    std::vector<size_t> osd_offsets;

    size_t osd_number = real_obj_number % (width);
    osd_offsets.push_back(osd_number);

    // TODO delete this buffer!!
    buf_redundance = new char[stripe_size];
    operations->push_back(
        ReadOperation(obj_number - i, osd_offsets, stripe_size, 0,
          buf_redundance));
    data_reads++;
    obj_number++;
    real_obj_number++;
  }

  while (start < size) {

    std::vector<size_t> osd_offsets;

    size_t osd_number = real_obj_number % (width);
    osd_offsets.push_back(osd_number);

    size_t req_size
      = min(size - start, static_cast<size_t>(stripe_size - offset));
    //is it a parity chunk?
    if (osd_number >= data_width) {
      buf_redundance = new char[stripe_size];
      // TODO fix comment here
      /* use the last object number used for a data stripe
       * this caters towards the calculation of a correct  filesize
       * ...and hopefully nothing else */
      operations->push_back(
          ReadOperation(obj_number - data_width, osd_offsets, stripe_size, 0,
              buf_redundance));
    } else {
      // operations->push_back(
      operations->insert(operations->begin() + data_reads,
          ReadOperation(obj_number, osd_offsets, req_size, offset,
            buf + start));
      offset = 0;
      data_reads++;
      start += req_size;
      obj_number++;
    }
    real_obj_number++;
    // if partial stripe at the end of the file
    if (size == start) {
      // size of incomplete object
      osd_offsets.clear();
      osd_offsets.push_back(data_width);
      if (obj_number % data_width == 1){
        // partial object at the start of the stripe
        // write data to parity object and enque par object with correct osd_offsets
        buf_redundance = new char[req_size];
        operations->push_back(
            ReadOperation(obj_number - 1, osd_offsets, req_size, 0,
              buf_redundance));
      } else {
        buf_redundance = new char[stripe_size];
        --obj_number;
        operations->push_back(
            ReadOperation(obj_number - (obj_number % data_width), osd_offsets, stripe_size, 0,
              buf_redundance));
        // full objects have been written in this line
        // create parity object and enque at correct position..remember N+1 striping
      }
    }

  }
  unsigned int lines = 1 + ((operations->size() - 1) / width);
  return operations->size() - lines;
}

size_t StripeTranslatorErasureCode::ProcessReads(
    std::vector<ReadOperation>* operations,
    boost::dynamic_bitset<>* successful_reads,
    PolicyContainer policies,
    size_t received_data,
    int64_t offset) const {
  // stripe size is stored in kB
  unsigned int stripe_size = (*policies.begin())->stripe_size() * 1024;
  // number of OSD to distribute parity chunks
  unsigned int parity_width = (*policies.begin())->parity_width();
  // number of OSD to distribute chunks
  unsigned int width = (*policies.begin())->width();
  // number of data stripes
  unsigned int data_width = width - parity_width;
  // how many (poss. incomplete)  lines does this operation contain
  // (x + y - 1) / y is apperantly a common idiom for quick ceil(x / y) can overflow in x + y though
  // 1 + ((x - 1) / y avoids overflow problem
  unsigned int lines = 1 + ((operations->size() - 1) / width);

  assert(operations->size() == successful_reads->size());

  if (((*successful_reads) << lines).count() == operations->size() - lines) {
    // if only data objects have been read cleanup and exit...nothing to do
    for (size_t i = operations->size() - lines; i < operations->size(); i++){
      delete (*operations)[i].data;
    }
    for (int i = 0; i < (offset / stripe_size) % data_width; i++) {
      delete (*operations)[i].data;
    }
    return subtract_extra_reads(received_data, offset, stripe_size, width, parity_width);
  }


  int real_size_of_last_read = received_data;
  for (size_t i = 0; i < lines; i++) {

    // create successful_reads bitvector for each line (data stripes + parity stripes)

    std::vector<ReadOperation> succ_ops;
    ReadOperation* failed_op = NULL;
    boost::dynamic_bitset<> line_mask(0);
    // normal reads
    unsigned int line_ops = min(i * data_width + data_width, successful_reads->size() - lines);
    for (size_t j = i * data_width; j < line_ops ; j++){
      line_mask.push_back(successful_reads->test(j));
      if (successful_reads->test(j)) {
        succ_ops.push_back((*operations)[j]);
        real_size_of_last_read -= (*operations)[j].req_size;
      } else {
        failed_op = &((*operations)[j]);
      }
    }
    // parity reads
    // parity reads are at the end of the successful_reads vector because they are read iff normal
    // reads have failed
    line_mask.push_back(successful_reads->test(successful_reads->size() - lines + i));
    succ_ops.push_back((*operations)[successful_reads->size() - lines + i]);
    real_size_of_last_read -= (*operations)[successful_reads->size() - lines + i].req_size;

    // check is enough data exists...at least data_width stripes must be present
    if (line_mask.count() < line_mask.size() - 1)
      throw IOException("to many failed reads");

    // if all data stripes have been read there is nothing todo
    if((line_mask << parity_width).count() == line_mask.size() - 1){
      continue;
    }

    if (failed_op) {
      size_t failed_size = failed_op->req_size;
      size_t failed_offset = failed_op->req_offset;
      char *failed_buf = (*failed_op).data;
      memset(failed_buf, 0, failed_size);
      // copy fixed data to correct location
      received_data -= failed_offset;
      for (size_t j = 0; j < succ_ops.size(); j++){
        char *d = succ_ops[j].data;
        size_t req_offset = succ_ops[j].req_offset;
        size_t op_size = min(succ_ops[j].req_size, failed_size);// min(failed_size, real_size_of_last_read));
        if (j < succ_ops.size() - 1 && real_size_of_last_read < 0) {
          if (op_size >= abs(real_size_of_last_read)) {
            op_size += real_size_of_last_read;
          } else {
            op_size = 0;
          }
        }
        for (int k = 0; k < op_size; k++){
          failed_buf[k] = d[k + failed_offset - req_offset] ^ failed_buf[k];
        }
      }
    }
  }

  // free parity buffers
  for (size_t i = operations->size() - lines; i < operations->size(); i++){
    delete (*operations)[i].data;
  }
  for (int i = 0; i < (offset / stripe_size) % data_width; i++) {
    delete (*operations)[i].data;
  }
  return subtract_extra_reads(received_data, offset, stripe_size, width, parity_width);
}

size_t StripeTranslatorErasureCode::subtract_extra_reads (
    size_t received_data,
    int64_t offset,
    unsigned int stripe_size,
    unsigned int width,
    unsigned int parity_width) const {

  unsigned int data_width = width - parity_width;

  if (offset < stripe_size) {
    return received_data;
  }

  unsigned int obj_offset = offset / stripe_size;
  unsigned int line_offset = obj_offset % data_width;
  return received_data - stripe_size * line_offset;
}

}  // namespace xtreemfs