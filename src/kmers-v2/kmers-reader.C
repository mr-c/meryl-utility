
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

#include "kmers.H"

namespace merylutil::inline kmers::v2 {

//  Clear all members and allocate buffers.
void
merylFileReader::initializeFromMasterI_v00(void) {

  _block         = new merylFileBlockReader();
  _blockIndex    = nullptr;

  _nKmers        = 0;
  _nKmersMax     = 1024;
  _suffixes      = new kmdata [_nKmersMax];
  _values        = new kmvalu [_nKmersMax];
  _labels        = new kmlabl [_nKmersMax];
}



//  Initialize for the original.
void
merylFileReader::initializeFromMasterI_v01(stuffedBits  *masterIndex,
                                           bool          doInitialize) {

  if (doInitialize == true) {
    initializeFromMasterI_v00();

    _prefixSize    = masterIndex->getBinary(32);
    _suffixSize    = masterIndex->getBinary(32);

    _numFilesBits  = masterIndex->getBinary(32);
    _numBlocksBits = masterIndex->getBinary(32);

    _numFiles      = (uint64)1 << _numFilesBits;    //  The same for all formats, but
    _numBlocks     = (uint64)1 << _numBlocksBits;   //  awkward to do outside of here.
  }

  //  If we didn't initialize, set the file position to the start
  //  of the statistics.
  else {
    masterIndex->setPosition(64 + 64 + 32 + 32 + 32 + 32);
  }
}



//  Initialize for the format that includes multi sets.
void
merylFileReader::initializeFromMasterI_v02(stuffedBits  *masterIndex,
                                           bool          doInitialize) {

  if (doInitialize == true) {
    initializeFromMasterI_v00();

    _prefixSize    = masterIndex->getBinary(32);
    _suffixSize    = masterIndex->getBinary(32);

    _numFilesBits  = masterIndex->getBinary(32);
    _numBlocksBits = masterIndex->getBinary(32);

    uint32 flags   = masterIndex->getBinary(32);

    _isMultiSet    = flags & (uint32)0x0001;        //  This is new in v02.

    _numFiles      = (uint64)1 << _numFilesBits;    //  The same for all formats, but
    _numBlocks     = (uint64)1 << _numBlocksBits;   //  awkward to do outside of here.
  }

  //  If we didn't initialize, set the file position to the start
  //  of the statistics.
  else {
    masterIndex->setPosition(64 + 64 + 32 + 32 + 32 + 32 + 32);
  }
}



void
merylFileReader::initializeFromMasterI_v03(stuffedBits  *masterIndex,
                                           bool          doInitialize) {
  initializeFromMasterI_v02(masterIndex, doInitialize);
}



void
merylFileReader::initializeFromMasterI_v04(stuffedBits  *masterIndex,
                                           bool          doInitialize) {
  initializeFromMasterI_v02(masterIndex, doInitialize);
}



void
merylFileReader::initializeFromMasterIndex(bool  doInitialize,
                                           bool  loadStatistics,
                                           bool  beVerbose) {
  char   N[FILENAME_MAX+1];

  snprintf(N, FILENAME_MAX, "%s/merylIndex", _inName);

  if (fileExists(N) == false)
    fprintf(stderr, "ERROR: '%s' doesn't appear to be a meryl input; file '%s' doesn't exist.\n",
            _inName, N), exit(1);

  //  Open the master index.

  stuffedBits  *masterIndex = new stuffedBits(N);

  //  Based on the magic number, initialzie.

  uint64  m1 = masterIndex->getBinary(64);
  uint64  m2 = masterIndex->getBinary(64);
  uint32  vv = 1;

  if        ((m1 == 0x646e496c7972656dllu) &&   //  merylInd
             (m2 == 0x31302e765f5f7865llu)) {   //  ex__v.01
    initializeFromMasterI_v01(masterIndex, doInitialize);
    vv = 1;

  } else if ((m1 == 0x646e496c7972656dllu) &&   //  merylInd
             (m2 == 0x32302e765f5f7865llu)) {   //  ex__v.02
    initializeFromMasterI_v02(masterIndex, doInitialize);
    vv = 2;

  } else if ((m1 == 0x646e496c7972656dllu) &&   //  merylInd
             (m2 == 0x33302e765f5f7865llu)) {   //  ex__v.03
    initializeFromMasterI_v03(masterIndex, doInitialize);
    vv = 3;

  } else if ((m1 == 0x646e496c7972656dllu) &&   //  merylInd
             (m2 == 0x34302e765f5f7865llu)) {   //  ex__v.04
    initializeFromMasterI_v04(masterIndex, doInitialize);
    vv = 4;

  } else {
    fprintf(stderr, "ERROR: '%s' doesn't look like a meryl input; file '%s' fails magic number check.\n",
            _inName, N), exit(1);
  }

  //  Check that the mersize is set and valid.

  uint32  merSize = (_prefixSize + _suffixSize) / 2;

  if (kmer::merSize() == 0)         //  If the global kmer size isn't set yet,
    kmer::setSize(merSize);         //  set it.

  if (kmer::merSize() != merSize)   //  And if set, make sure we're compatible.
    fprintf(stderr, "mer size mismatch, can't process this set of files.\n"), exit(1);

  //  If loading statistics is enabled, load the stats assuming the file is in
  //  the proper position.

  if (loadStatistics == true) {
    _stats = new merylHistogram;
    _stats->load(masterIndex, vv);
  }

  //  And report some logging.

  if (beVerbose) {
    char    m[17] = { 0 };

    for (uint32 i=0, s=0; i<8; i++, s+=8) {
      m[i + 0] = (m1 >> s) & 0xff;
      m[i + 8] = (m2 >> s) & 0xff;
    }

    fprintf(stderr, "Opened '%s'.\n", _inName);
    fprintf(stderr, "  magic          0x%016lx%016lx '%s'\n", m1, m2, m);
    fprintf(stderr, "  prefixSize     %u\n", _prefixSize);
    fprintf(stderr, "  suffixSize     %u\n", _suffixSize);
    fprintf(stderr, "  numFilesBits   %u (%u files)\n", _numFilesBits, _numFiles);
    fprintf(stderr, "  numBlocksBits  %u (%u blocks)\n", _numBlocksBits, _numBlocks);
  }

  delete masterIndex;
}



merylFileReader::merylFileReader(const char *inputName,
                                 bool        beVerbose) {
  strncpy(_inName, inputName, FILENAME_MAX);
  initializeFromMasterIndex(true, false, beVerbose);
}



merylFileReader::merylFileReader(const char *inputName,
                                 uint32      threadFile,
                                 bool        beVerbose) {
  strncpy(_inName, inputName, FILENAME_MAX);
  initializeFromMasterIndex(true, false, beVerbose);
  enableThreads(threadFile);
}



merylFileReader::~merylFileReader() {

  delete [] _blockIndex;

  delete [] _suffixes;
  delete [] _values;
  delete [] _labels;

  delete    _stats;

  merylutil::closeFile(_datFile);

  delete    _block;
}



void
merylFileReader::loadStatistics(void) {
  if (_stats == NULL)
    initializeFromMasterIndex(false, true, false);
}



void
merylFileReader::dropStatistics(void) {
  delete _stats;
  _stats = NULL;
}



void
merylFileReader::enableThreads(uint32 threadFile) {
  _activeFile = threadFile;
  _threadFile = threadFile;
}



void
merylFileReader::loadBlockIndex(void) {

  if (_blockIndex != NULL)
    return;

  _blockIndex = new merylFileIndex [_numFiles * _numBlocks];

  for (uint32 ii=0; ii<_numFiles; ii++) {
    char  *idxname = constructBlockName(_inName, ii, _numFiles, 0, true);
    FILE  *idxfile = merylutil::openInputFile(idxname);

    loadFromFile(_blockIndex + _numBlocks * ii, "merylFileReader::blockIndex", _numBlocks, idxfile);

    merylutil::closeFile(idxfile, idxname);

    delete [] idxname;
  }
}



bool
merylFileReader::nextMer(void) {

  _activeMer++;

  //  If we've still got data, just update and get outta here.
  //  Otherwise, we need to load another block.

  if (_activeMer < _nKmers) {
    _kmer.setPrefixSuffix(_prefix, _suffixes[_activeMer], _suffixSize);
    _kmer._val = _values[_activeMer];
    return(true);
  }

  //  If no file, open whatever is 'active'.  In thread mode, the first file
  //  we open is the 'threadFile'; in normal mode, the first file we open is
  //  the first file in the database.

 loadAgain:
  if (_datFile == NULL)
    _datFile = openInputBlock(_inName, _activeFile, _numFiles);

  //  Load blocks.

  bool loaded = _block->loadKmerFileBlock(_datFile, _activeFile);

  //  If nothing loaded. open a new file and try again.

  if (loaded == false) {
    merylutil::closeFile(_datFile);

    if (_activeFile == _threadFile)   //  Thread mode, if no block was loaded,
      return(false);                  //  we're done.

    _activeFile++;

    if (_numFiles <= _activeFile)
      return(false);

    goto loadAgain;
  }

  //  Got a block!  Stash what we loaded.

  _prefix = _block->prefix();
  _nKmers = _block->nKmers();

#ifdef SHOW_LOAD
  fprintf(stdout, "LOADED prefix %016lx nKmers %lu\n", _prefix, _nKmers);
#endif

  //  Make sure we have space for the decoded data

  resizeArray(_suffixes, _values, _labels, 0, _nKmersMax, _nKmers, _raAct::doNothing);

  //  Decode the block into _OUR_ space.
  //
  //  decodeKmerFileBlock() marks the block as having no data, so the next
  //  time we loadBlock() it will read more data from disk.  For blocks that
  //  don't get decoded, they retain whatever was loaded, and do not load
  //  another block in loadBlock().

  _block->decodeKmerFileBlock(_suffixes, _values, _labels);

  //  But if no kmers in this block, load another block.  Sadly, the block must always
  //  be decoded, otherwise, the load will not load a new block.

  if (_nKmers == 0)
    goto loadAgain;

  //  Reset iteration, and load the first kmer.

  _activeMer = 0;

  _kmer.setPrefixSuffix(_prefix, _suffixes[_activeMer], _suffixSize);
  _kmer._val = _values[_activeMer];
  _kmer._lab = _labels[_activeMer];

  return(true);
}

}  //  namespace merylutil::kmers::v2
