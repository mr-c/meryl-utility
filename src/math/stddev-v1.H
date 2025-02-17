
/******************************************************************************
 *
 *  This file is part of meryl-utility, a collection of miscellaneous code
 *  used by Meryl, Canu and others.
 *
 *  This software is based on:
 *    'Canu' v2.0              (https://github.com/marbl/canu)
 *  which is based on:
 *    'Celera Assembler' r4587 (http://wgs-assembler.sourceforge.net)
 *    the 'kmer package' r1994 (http://kmer.sourceforge.net)
 *
 *  Except as indicated otherwise, this is a 'United States Government Work',
 *  and is released in the public domain.
 *
 *  File 'README.licenses' in the root directory of this distribution
 *  contains full conditions and disclaimers.
 */

#ifndef MERYLUTIL_MATH_STDDEV_V1_H
#define MERYLUTIL_MATH_STDDEV_V1_H

#include "types.H"
#include "arrays.H"

#include <vector>
#include <algorithm>

namespace merylutil::inline math::inline v1 {


//  Online mean and std.dev calculation.
//  B. P. Welford, Technometrics, Vol 4, No 3, Aug 1962 pp 419-420.
//    http://www2.in.tu-clausthal.de/~zach/teaching/info_literatur/Welford.pdf
//  Also presented in Knuth Vol 2 (3rd Ed.) pp 232.
//
template<typename TT>
class stdDev {
public:
  stdDev(double mn=0.0, double sn=0.0, uint32 nn=0) {
    _mn = mn;
    _sn = sn;
    _nn = nn;
  };

  ~stdDev() {
  };

  void     insert(TT val) {
    double m0 = _mn;
    double s0 = _sn;
    uint32 n0 = _nn + 1;

    if (_nn == 0x7fffffff)
      fprintf(stderr, "ERROR: stdDev is full; can't insert() new value.\n"), exit(1);

    if (_nn & 0x80000000)
      fprintf(stderr, "ERROR: stdDev has been finalized; can't insert() new value.\n"), exit(1);

    _mn = m0 + (val - m0) / n0;
    _sn = s0 + (val - m0) * (val - _mn);
    _nn = n0;
  };

  void     remove(TT val) {
    uint32 n0 = _nn - 1;
    double m0 = (n0 == 0) ? (0) : ((_nn * _mn - val) / n0);
    double s0 = _sn - (val - m0) * (val - _mn);

    if (n0 == 0)   m0 = 0.0;   //  Reset mean and variance to zero when we can.
    if (n0 <= 1)   s0 = 0.0;   //  See tests/stddevTest.C testStability() for details.

    if (s0 < 0.0)              //  Assume negative values are due to stability problems,
      s0 = 0.0;                //  and not mismatched insert() and delete() values.
    if (-1e-10 <= m0 && m0 <= 1e-10)
      m0 = 0.0;

    if (_nn == 0)
      fprintf(stderr, "ERROR: stdDev has no data; can't remove() old value.\n"), exit(1);

    if (_nn & 0x80000000)
      fprintf(stderr, "ERROR: stdDev has been finalized; can't remove() old value.\n"), exit(1);

    _nn = n0;
    _mn = m0;
    _sn = s0;
  };

  void     finalize(void) {
    _sn  = stddev();
    _nn  |= 0x80000000;
  };

  uint32   size(void) {
    return(_nn & 0x7fffffff);
  };

  double   mean(void) {
    return(_mn);
  };

  double   variance(void) {
    if (_nn & 0x80000000)
      return(_sn * _sn);
    else
      return((_nn < 2) ? (0.0) : (_sn / (_nn-1)));
  };

  double   stddev(void) {
    if (_nn & 0x80000000)
      return(_sn);
    else
      return(sqrt(variance()));
  };

private:
  double   _mn;  //  mean
  double   _sn;  //  "sum of variances"
  uint32   _nn;  //  number of items in the set
};




//  Offline mean and std.dev calculation.  Filters outliers.
//  Does not work well with unsigned types.  The 'smallest' compute can underflow.
//
template<typename TT>
void
computeStdDev(TT *dist, uint64 distLen, double &mean, double &stddev, bool isSorted=false) {
  mean   = 0;
  stddev = 0;

  if (distLen == 0)
    return;

  //  Sort the values.  Lets us approximate the stddev for filtering out outliers.

  if (isSorted == false)
    std::sort(dist, dist + distLen);

  //  Approximate the stddev to filter out outliers.  This is done by assuming we're normally
  //  distributed, finding the values that would represent 1 standard deviation (about 68.27% of the
  //  data), and using that to find the 5 std.dev. limits.

  TT median     = dist[1 * distLen / 2];

  TT oneThird   = dist[1 * distLen / 3];
  TT twoThird   = dist[2 * distLen / 3];

  TT approxStd  = max(median - oneThird, twoThird - median);

  TT biggest    = median + approxStd * 5;
  TT smallest   = median - approxStd * 5;

  fprintf(stderr, "computeStdDev  %d %d %d %d %d %d\n", median, oneThird, twoThird, approxStd, biggest, smallest);

  //  Now, compute the number of samples within our bounds.  And find the mean, too.

  size_t  numSamples = 0;

  for (size_t x=0; x<distLen; x++)
    if ((smallest  <= dist[x]) &&
        (dist[x]   <= biggest)) {
      numSamples += 1;
      mean       += dist[x];
    }

  if (numSamples == 0)
    return;

  mean   = mean / numSamples;

  //  Use the standard std.dev. algorithm, tossing out the outliers.

  for (uint64 x=0; x<distLen; x++)
    if ((smallest  <= dist[x]) &&
        (dist[x]   <= biggest))
      stddev += (dist[x] - mean) * (dist[x] - mean);

  if (numSamples > 1)
    stddev = sqrt(stddev / (numSamples - 1));
};

template<typename TT>
void
computeStdDev(std::vector<TT> dist, double &mean, double &stddev, bool isSorted=false) {
  computeStdDev(dist.data(), dist.size(), mean, stddev, isSorted);
}



//  Compute the mode.  Once the values are sorted, we just need to scan the list and remember the
//  most common value.
//
template<typename TT>
void
computeMode(TT *dist, uint64 distLen, TT &mode, bool isSorted=false) {
  mode = 0;

  if (distLen == 0)
    return;

  if (isSorted == false)
    std::sort(dist, dist + distLen);

  uint32  modeCnt = 0;
  TT      modeVal = 0;

  uint32  modeTmpCnt = 0;
  TT      modeTmpVal = 0;

  for (uint64 x=0; x<distLen; x++) {
    if (dist[x] != modeTmpVal) {
      if (modeCnt < modeTmpCnt) {
        modeCnt = modeTmpCnt;
        modeVal = modeTmpVal;
      }

      modeTmpCnt = 1;
      modeTmpVal = dist[x];
    }

    modeTmpCnt++;
  }

  if (modeCnt < modeTmpCnt) {
    modeCnt = modeTmpCnt;
    modeVal = modeTmpVal;
  }

  mode = modeVal;
}

template<typename TT>
void
computeMode(std::vector<TT> dist, TT &mode, bool isSorted=false) {
  computeMode(dist.data(), dist.size(), mode, isSorted);
}



template<typename TT>
void
computeMedian(TT *dist, uint64 distLen, TT &median, bool isSorted=false) {
  median = 0;

  if (distLen == 0)
    return;

  if (isSorted == false)
    std::sort(dist, dist + distLen);

  if (distLen % 2 == 0)
     median = (dist[distLen / 2 - 1] + dist[distLen / 2]) / 2;
  else
     median = dist[distLen / 2];
}

template<typename TT>
void
computeMedian(std::vector<TT> dist, TT &median, bool isSorted=false) {
  computeMedian(dist.data(), dist.size(), median, isSorted);
}

//  Compute the median and median absolute deviation.  Sort the values to find the median, then
//  build a new vector of |median - x| and find the median of that.
//
template<typename TT>
void
computeMedianAbsoluteDeviation(TT *dist, uint64 distLen, TT &median, TT &mad, bool isSorted=false) {
  median = 0;
  mad    = 0;

  if (distLen == 0)
    return;

  if (isSorted == false)
    std::sort(dist, dist + distLen);

  computeMedian(dist, distLen, median, true);

  std::vector<TT>  m;

  for (uint64 ii=0; ii<distLen; ii++) {
    if (dist[ii] < median)
      m.push_back(median - dist[ii]);
    else
      m.push_back(dist[ii] - median);
  }

  std::sort(m.begin(), m.end());

  mad = m[ m.size()/2 ];
};

template<typename TT>
void
computeMedianAbsoluteDeviation(std::vector<TT> dist, TT &median, TT &mad, bool isSorted=false) {
  computeMedianAbsoluteDeviation(dist.data(), dist.size(), median, mad, isSorted);
}



template<typename TT>
TT
computeExponentialMovingAverage(TT alpha, TT ema, TT value) {
  assert(0.0   <= alpha);
  assert(alpha <= 1.0);

  return(alpha * value + (1 - alpha) * ema);
};







class histogramStatistics {
public:
  histogramStatistics() {
    _histogramAlloc = 1024 * 1024;
    _histogramMax = 0;
    _histogram    = new uint64 [_histogramAlloc];

    memset(_histogram, 0, sizeof(uint64) * _histogramAlloc);

    _finalized = false;

    clearStatistics();
  };
  ~histogramStatistics() {
    delete [] _histogram;
  };

  void               add(uint64 data, uint32 count=1) {
    while (_histogramAlloc <= data)
      resizeArray(_histogram, _histogramMax+1, _histogramAlloc, _histogramAlloc * 2, _raAct::copyData | _raAct::clearNew);

    if (_histogramMax < data)
      _histogramMax = data;

    _histogram[data] += count;
    _finalized = false;
  };


  uint64             numberOfObjects(void)  { finalizeData(); return(_numObjs);  };

  double             mean(void)             { finalizeData(); return(_mean);     };
  double             stddev(void)           { finalizeData(); return(_stddev);   };

  uint64             median(void)           { finalizeData(); return(_median);   };
  uint64             mad(void)              { finalizeData(); return(_mad);      };

  void               clearStatistics(void) {
    _numObjs  = 0;

    _mean     = 0.0;
    _stddev   = 0.0;

    _mode     = 0;

    _median   = 0;
    _mad      = 0;
  };

  void               finalizeData(void) {

    if (_finalized == true)
      return;

    //  Cheat sheet for this function.
    //    ii is the value of a sample item
    //    _histogram[ii] is how many of each item we have
    //  So:
    //  a)  Something like '_histogram[ii] * f(ii)' we're just adding
    //      the contributions of each object.
    //  b)  Pretend '_histogram[ii]' is 1 and the usual algorithms
    //      should appear.

    clearStatistics();

    //  Compute number of objects

    for (uint64 ii=0; ii <= _histogramMax; ii++)
      _numObjs += _histogram[ii];

    //  Compute mean and stddev

    for (uint64 ii=0; ii <= _histogramMax; ii++)
      _mean += ii * _histogram[ii];

    if (_numObjs > 1)
      _mean /= _numObjs;

    for (uint64 ii=0; ii <= _histogramMax; ii++)
      _stddev += _histogram[ii] * (ii - _mean) * (ii - _mean);

    if (_numObjs > 1)
      _stddev = sqrt(_stddev / (_numObjs - 1));

    //  Compute mode, pick the highest to break ties

    for (uint64 ii=0; ii <= _histogramMax; ii++)
      if (_histogram[ii] > _histogram[_mode])
        _mode = ii;

    //  Compute median and mad
    //
    //  The MAD is the 'median of the absolute deviations from the set median'.
    //  We can compute this by making another histogram collection of data,
    //  this time of the absolute deviations.  Then just find the median of
    //  that, as above.

    for (uint64 ii=0; ii <= _histogramMax; ii++) {
      _median += _histogram[ii];       //  Sum the number of objects we've seen.

      if (_median >= _numObjs / 2) {   //  If we've seen half of them, set the
        _median = ii;                  //  median to the value we're at and
        break;                         //  stop.
      }
    }

    uint64   maddatamax = _histogramMax + 1;        //  Needs every value.  Consider [0]=big, [n]=1.
    uint64  *maddata    = new uint64 [maddatamax];  //  Deviation from median will be n - 0.

    for (uint64 ii=0; ii < maddatamax; ii++)
      maddata[ii] = 0;

    for (uint64 ii=0; ii <= _histogramMax; ii++) {
      if (_histogram[ii] > 0) {
        uint64  deviation = (ii < _median) ? (_median - ii) : (ii - _median);

        if (deviation >= maddatamax) {
          fprintf(stderr, "finalizeData()--  Failed at ii=%lu for _histogramMax=%lu\n", ii, _histogramMax);
          fprintf(stderr, "finalizeData()--            _median=%lu\n", _median);
          fprintf(stderr, "finalizeData()--            deviation=%lu\n", deviation);
        }
        assert(deviation < maddatamax);

        maddata[deviation] += _histogram[ii];
      }
    }

    for (uint64 ii=0; ii <= _histogramMax; ii++) {
      _mad += maddata[ii];

      if (_mad >= _numObjs / 2) {
        _mad = ii;
        break;
      }
    }

    //  And, done

    delete [] maddata;

    _finalized = true;
  };

  uint64           histogram(uint64 ii) {
    return(_histogram[ii]);
  };
  uint64           histogramMax(void) {
    return(_histogramMax);
  };

  void             writeHistogram(FILE *F, char *label) {
    fprintf(F, "#%s\tquantity\n", label);

    for (uint64 ii=0; ii <= _histogramMax; ii++)
      fprintf(F, F_U64"\t" F_U64 "\n", ii, _histogram[ii]);
  };

private:
  bool            _finalized;

  uint64          _histogramAlloc;  //  Maximum allocated value
  uint64          _histogramMax;    //  Maximum valid value
  uint64         *_histogram;

  uint64          _numObjs;

  double          _mean;
  double          _stddev;

  uint64          _mode;

  uint64          _median;
  uint64          _mad;
};

}  //  merylutil::files::v1

#endif  //  MERYLUTIL_MATH_STDDEV_V1_H
