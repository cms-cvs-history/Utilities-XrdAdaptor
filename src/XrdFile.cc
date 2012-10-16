#include "Utilities/XrdAdaptor/src/XrdFile.h"
#include "FWCore/Utilities/interface/EDMException.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Utilities/interface/Likely.h"
#include <vector>
#include <sstream>

// To be re-enabled when the monitoring interface is back.
//static const char *kCrabJobIdEnv = "CRAB_UNIQUE_JOB_ID";

#define XRD_CL_MAX_CHUNK 512*1024

XrdFile::XrdFile (void)
  : m_file (nullptr),
    m_offset (0),
    m_size(-1),
    m_close (false),
    m_name()
{
}

XrdFile::XrdFile (const char *name,
    	          int flags /* = IOFlags::OpenRead */,
    	          int perms /* = 066 */)
  : m_file (nullptr),
    m_offset (0),
    m_size(-1),
    m_close (false),
    m_name()
{
  open (name, flags, perms);
}

XrdFile::XrdFile (const std::string &name,
    	          int flags /* = IOFlags::OpenRead */,
    	          int perms /* = 066 */)
  : m_file (nullptr),
    m_offset (0),
    m_size(-1),
    m_close (false),
    m_name()
{
  open (name.c_str (), flags, perms);
}

XrdFile::~XrdFile (void)
{
  if (m_close)
    edm::LogError("XrdFileError")
      << "Destructor called on XROOTD file '" << m_name
      << "' but the file is still open";
}

//////////////////////////////////////////////////////////////////////
void
XrdFile::create (const char *name,
		 bool exclusive /* = false */,
		 int perms /* = 066 */)
{
  open (name,
        (IOFlags::OpenCreate | IOFlags::OpenWrite | IOFlags::OpenTruncate
         | (exclusive ? IOFlags::OpenExclusive : 0)),
        perms);
}

void
XrdFile::create (const std::string &name,
                 bool exclusive /* = false */,
                 int perms /* = 066 */)
{
  open (name.c_str (),
        (IOFlags::OpenCreate | IOFlags::OpenWrite | IOFlags::OpenTruncate
         | (exclusive ? IOFlags::OpenExclusive : 0)),
        perms);
}

void
XrdFile::open (const std::string &name,
               int flags /* = IOFlags::OpenRead */,
               int perms /* = 066 */)
{ open (name.c_str (), flags, perms); }

void
XrdFile::open (const char *name,
               int flags /* = IOFlags::OpenRead */,
               int perms /* = 066 */)
{
  // Actual open
  if ((name == 0) || (*name == 0)) {
    edm::Exception ex(edm::errors::FileOpenError);
    ex << "Cannot open a file without a name";
    ex.addContext("Calling XrdFile::open()");
    throw ex;
  }
  if ((flags & (IOFlags::OpenRead | IOFlags::OpenWrite)) == 0) {
    edm::Exception ex(edm::errors::FileOpenError);
    ex << "Must open file '" << name << "' at least for read or write";
    ex.addContext("Calling XrdFile::open()");
    throw ex;
  }
  // If I am already open, close old file first
  if (m_file.get())
  {
    if (m_close)
      close();
    else
      abort();
  }

  // Translate our flags to system flags
  int openflags = 0;

  if (flags & IOFlags::OpenWrite)
    openflags |= XrdCl::OpenFlags::Update;
  else if (flags & IOFlags::OpenRead)
    openflags |= XrdCl::OpenFlags::Read;

  if (flags & IOFlags::OpenAppend) {
    edm::Exception ex(edm::errors::FileOpenError);
    ex << "Opening file '" << name << "' in append mode not supported";
    ex.addContext("Calling XrdFile::open()");
    throw ex;
  }

  if (flags & IOFlags::OpenCreate)
  {
    if (! (flags & IOFlags::OpenExclusive))
      openflags |= XrdCl::OpenFlags::Delete;
    openflags |= XrdCl::OpenFlags::New;
    openflags |= XrdCl::OpenFlags::MakePath;
  }

  if ((flags & IOFlags::OpenTruncate) && (flags & IOFlags::OpenWrite))
    openflags |= XrdCl::OpenFlags::Delete;

  m_name = name;
  m_file.reset(new XrdCl::File());
  XrdCl::XRootDStatus status;
  if (! (status = m_file->Open(name, openflags, perms)).IsOK()) {
    edm::Exception ex(edm::errors::FileOpenError);
    ex << "XrdCl::File::Open(name='" << name
       << "', flags=0x" << std::hex << openflags
       << ", permissions=0" << std::oct << perms << std::dec
       << ") => error '" << status.ToString()
       << "' (errno=" << status.errNo << ", code=" << status.code << ")";
    ex.addContext("Calling XrdFile::open()");
    addConnection(ex);
    throw ex;
  }
  XrdCl::StatInfo *statInfo = NULL;
  if (! (status = m_file->Stat(true, statInfo)).IsOK()) {
    edm::Exception ex(edm::errors::FileOpenError);
    ex << "XrdCl::File::Stat(name='" << name
      << ") => error '" << status.ToString()
      << "' (errno=" << status.errNo << ", code=" << status.code << ")";
    ex.addContext("Calling XrdFile::open()");
    addConnection(ex);
    throw ex;
  }
  assert(statInfo);
  m_size = statInfo->GetSize();
  delete(statInfo);
  m_offset = 0;
  m_close = true;

  // Send the monitoring info, if available.
  // Note: getenv is not reentrant.
  // Commenting out until this is available in the new client.
/*
  char * crabJobId = getenv(kCrabJobIdEnv);
  if (crabJobId) {
    kXR_unt32 dictId;
    m_file->SendMonitoringInfo(crabJobId, &dictId);
    edm::LogInfo("XrdFileInfo") << "Set monitoring ID to " << crabJobId << " with resulting dictId " << dictId << ".";
  }
*/

  edm::LogInfo("XrdFileInfo") << "Opened " << m_name;

  edm::LogInfo("XrdFileInfo") << "Connection URL " << m_file->GetDataServer();
}

void
XrdFile::close (void)
{
  if (! m_file.get())
  {
    edm::LogError("XrdFileError")
      << "XrdFile::close(name='" << m_name
      << "') called but the file is not open";
    m_close = false;
    return;
  }

  XrdCl::XRootDStatus status;
  if (! (status = m_file->Close()).IsOK())
    edm::LogWarning("XrdFileWarning")
      << "XrdFile::close(name='" << m_name
      << "') failed with error '" << status.ToString()
      << "' (errno=" << status.errNo << ", code=" << status.code << ")";
  m_file.reset(0);

  m_close = false;
  m_offset = 0;
  m_size = -1;
  edm::LogInfo("XrdFileInfo") << "Closed " << m_name;
}

void
XrdFile::abort (void)
{
  m_file.reset(0);
  m_close = false;
  m_offset = 0;
  m_size = -1;
}

//////////////////////////////////////////////////////////////////////
IOSize
XrdFile::read (void *into, IOSize n)
{
  if (n > 0x7fffffff) {
    edm::Exception ex(edm::errors::FileReadError);
    ex << "XrdFile::read(name='" << m_name << "', n=" << n
       << ") too many bytes, limit is 0x7fffffff";
    ex.addContext("Calling XrdFile::read()");
    addConnection(ex);
    throw ex;
  }
  uint32_t bytesRead;
  XrdCl::XRootDStatus s = m_file->Read(m_offset, n, into, bytesRead);
  if (!s.IsOK()) {
    edm::Exception ex(edm::errors::FileReadError);
    ex << "XrdClient::Read(name='" << m_name
       << "', offset=" << m_offset << ", n=" << n
       << ") failed with error '" << s.ToString()
       << "' (errno=" << s.errNo << ", code=" << s.code << ")";
    ex.addContext("Calling XrdFile::read()");
    addConnection(ex);
    throw ex;
  }
  m_offset += bytesRead;
  return bytesRead;
}

IOSize
XrdFile::read (void *into, IOSize n, IOOffset pos)
{
  if (n > 0x7fffffff) {
    edm::Exception ex(edm::errors::FileReadError);
    ex << "XrdFile::read(name='" << m_name << "', n=" << n
       << ") exceeds read size limit 0x7fffffff";
    ex.addContext("Calling XrdFile::read()");
    addConnection(ex);
    throw ex;
  }
  uint32_t bytesRead;
  XrdCl::XRootDStatus s = m_file->Read(pos, n, into, bytesRead);
  if (!s.IsOK()) {
    edm::Exception ex(edm::errors::FileReadError);
    ex << "XrdClient::Read(name='" << m_name
       << "', offset=" << m_offset << ", n=" << n
       << ") failed with error '" << s.ToString()
       << "' (errno=" << s.errNo << ", code=" << s.code << ")";
    ex.addContext("Calling XrdFile::read()");
    addConnection(ex);
    throw ex;
  }
  return bytesRead;
}

// This method is rarely used by CMS; hence, it is a small wrapper and not efficient.
IOSize
XrdFile::readv (IOBuffer *into, IOSize n)
{
  std::vector<IOPosBuffer> new_buf;
  new_buf.reserve(n);
  IOOffset off = 0;
  for (IOSize i=0; i<n; i++) {
    IOSize size = into[i].size();
    new_buf[i] = IOPosBuffer(off, into[i].data(), size);
    off += size;
  }
  return readv(&(new_buf[0]), n);
}

/*
 * A vectored scatter-gather read.
 * Returns the total number of bytes successfully read.
 */
IOSize
XrdFile::readv (IOPosBuffer *into, IOSize n)
{
  assert(m_file.get());
  
  // A trivial vector read - unlikely, considering ROOT data format.
  if (unlikely(n == 0)) {
    return 0;
  }
  if (unlikely(n == 1)) {
    return read(into[0].data(), into[0].size(), into[0].offset());
  }

  XrdCl::ChunkList cl; cl.reserve(n);
  IOSize size = 0;
  for (IOSize i=0; i<n; i++) {
    IOOffset offset = into[i].offset();
    IOSize length = into[i].size();
    size += length;
    char * buffer = static_cast<char *>(into[i].data());
    while (length > XRD_CL_MAX_CHUNK) {
      XrdCl::ChunkInfo ci;
      ci.length = XRD_CL_MAX_CHUNK;
      length -= XRD_CL_MAX_CHUNK;
      ci.offset = offset;
      offset += XRD_CL_MAX_CHUNK;
      ci.buffer = buffer;
      buffer += XRD_CL_MAX_CHUNK;
      cl.push_back(ci);
    }
    XrdCl::ChunkInfo ci;
    ci.length = length;
    ci.offset = offset;
    ci.buffer = buffer;
    cl.push_back(ci);
  }
  XrdCl::VectorReadInfo *vr = nullptr;
  XrdCl::XRootDStatus s = m_file->VectorRead(cl, nullptr, vr);
  if (!s.IsOK()) {
    edm::Exception ex(edm::errors::FileReadError);
    ex << "XrdFile::readv(name='" << m_name
       << "', size=" << size << ", n=" << n
       << ") failed with error '" << s.ToString()
       << "' (errno=" << s.errNo << ", code=" << s.code << ")";
    ex.addContext("Calling XrdFile::readv()");
    addConnection(ex);
    throw ex;
  }
  assert(vr);
  return vr->GetSize();
}

IOSize
XrdFile::write (const void *from, IOSize n)
{
  if (n > 0x7fffffff) {
    cms::Exception ex("FileWriteError");
    ex << "XrdFile::write(name='" << m_name << "', n=" << n
       << ") too many bytes, limit is 0x7fffffff";
    ex.addContext("Calling XrdFile::write()");
    addConnection(ex);
    throw ex;
  }
  XrdCl::XRootDStatus s = m_file->Write(m_offset, n, from);
  if (!s.IsOK()) {
    cms::Exception ex("FileWriteError");
    ex << "XrdFile::write(name='" << m_name << "', n=" << n
       << ") failed with error '" << s.ToString()
       << "' (errno=" << s.errNo << ", code=" << s.code << ")";
    ex.addContext("Calling XrdFile::write()");
    addConnection(ex);
    throw ex;
  }
  m_offset += n;
  assert(m_size != -1);
  if (m_offset > m_size)
    m_size = m_offset;

  return n;
}

IOSize
XrdFile::write (const void *from, IOSize n, IOOffset pos)
{
  if (n > 0x7fffffff) {
    cms::Exception ex("FileWriteError");
    ex << "XrdFile::write(name='" << m_name << "', n=" << n
       << ") too many bytes, limit is 0x7fffffff";
    ex.addContext("Calling XrdFile::write()");
    addConnection(ex);
    throw ex;
  }
  // TODO: the current XrdCl API is such that short reads are not possible
  // on success.
  XrdCl::XRootDStatus s = m_file->Write(pos, n, from);
  if (!s.IsOK()) {
    cms::Exception ex("FileWriteError");
    ex << "XrdFile::write(name='" << m_name << "', n=" << n
       << ") failed with error '" << s.ToString()
       << "' (errno=" << s.errNo << ", code=" << s.code << ")";
    ex.addContext("Calling XrdFile::write()");
    addConnection(ex);
    throw ex;
  }
  assert (m_size != -1);
  if (static_cast<IOOffset>(pos + n) > m_size)
    m_size = pos + n;

  return n;
}

bool
XrdFile::prefetch (const IOPosBuffer *what, IOSize n)
{
  // The new Xrootd client does not contain any internal buffers.
  // Hence, prefetching is disabled completely.
  return false;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
IOOffset
XrdFile::position (IOOffset offset, Relative whence /* = SET */)
{
  if (! m_file.get()) {
    cms::Exception ex("FilePositionError");
    ex << "XrdFile::position() called on a closed file";
    ex.addContext("Calling XrdFile::position()");
    addConnection(ex);
    throw ex;
  }
  switch (whence)
  {
  case SET:
    m_offset = offset;
    break;

  case CURRENT:
    m_offset += offset;
    break;

  // TODO: None of this works with concurrent writers to the file.
  case END:
    assert(m_size != -1);
    m_offset = m_size + offset;
    break;

  default:
    cms::Exception ex("FilePositionError");
    ex << "XrdFile::position() called with incorrect 'whence' parameter";
    ex.addContext("Calling XrdFile::position()");
    addConnection(ex);
    throw ex;
  }

  if (m_offset < 0)
    m_offset = 0;
  assert(m_size != -1);
  if (m_offset > m_size)
    m_size = m_offset;

  return m_offset;
}

void
XrdFile::resize (IOOffset /* size */)
{
  cms::Exception ex("FileResizeError");
  ex << "XrdFile::resize(name='" << m_name << "') not implemented";
  ex.addContext("Calling XrdFile::resize()");
  addConnection(ex);
  throw ex;
}

void
XrdFile::addConnection (cms::Exception &ex)
{
  if (m_file.get()) {
    std::stringstream ss;
    ss << "Current server connection: " << m_file->GetDataServer();
    ex.addAdditionalInfo(ss.str());
  }
}

