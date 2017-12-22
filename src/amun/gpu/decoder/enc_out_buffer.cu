#include "enc_out_buffer.h"

using namespace std;

namespace amunmt {
namespace GPU {

EncOutBuffer::EncOutBuffer(unsigned int maxSize)
:buffer_(maxSize)
{
}

EncOutBuffer::~EncOutBuffer()
{
//  cerr << "~EncOutBuffer" << endl;
}

void EncOutBuffer::Add(EncOutPtr obj)
{
  buffer_.add(obj);
}

EncOutPtr EncOutBuffer::Get()
{
  return buffer_.remove();
}

void EncOutBuffer::Get(size_t num, std::vector<BufferOutput> &ret)
{
  for (size_t currNum = 0; currNum < num; ++currNum) {
    if (unfinishedEncOutPtr_.get() == nullptr) {
      unfinishedEncOutPtr_ = Get();
      unfinishedInd_ = 0;
    }

    if (unfinishedEncOutPtr_->GetSentences().size() == 0) {
      // no more sentences
      return;
    }

    assert(unfinishedEncOutPtr_);
    BufferOutput ele(unfinishedEncOutPtr_, unfinishedInd_);
    ret.push_back(ele);

    ++unfinishedInd_;

    const EncOut &encOut = *unfinishedEncOutPtr_;
    const Sentences &sentences = encOut.GetSentences();
    if (unfinishedInd_ == sentences.size()) {
      // last sentence
      unfinishedEncOutPtr_.reset();
    }

  }

}

}
}
