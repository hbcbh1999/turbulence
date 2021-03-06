#ifndef _NSEOF_PLOTTING_READER_H_
#define _NSEOF_PLOTTING_READER_H_

#include <string>

#include "../Parameters.h"
#include "../hdf5/HDF5.h"

namespace nseof {

namespace plotting {

class Reader {
 public:
  virtual int getDim() = 0;
  virtual std::string getName() = 0;
  virtual hid_t getHDF5Type() = 0;
  virtual hid_t getHDF5NativeType() = 0;

  /**
   * Read data from the flow field and write it into an hdf5 file
   */
  virtual void write(const hid_t dataset, Parameters& p,
                     nseof::hdf5::HDF5& hdf5) = 0;
};
}
}

#endif  // _NSEOF_PLOTTING_READER_H_
