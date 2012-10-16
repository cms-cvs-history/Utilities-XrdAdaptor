#ifndef Utilities_XrdAdaptor_XrdFile_h
#define Utilities_XrdAdaptor_XrdFile_h

# include "Utilities/StorageFactory/interface/Storage.h"
# include "Utilities/StorageFactory/interface/IOFlags.h"
# include "FWCore/Utilities/interface/Exception.h"
# include "XrdCl/XrdClFile.hh"
# include <string>
# include <memory>

class XrdFile : public Storage
{
public:
  XrdFile (void);
  XrdFile (IOFD fd);
  XrdFile (const char *name, int flags = IOFlags::OpenRead, int perms = 0666);
  XrdFile (const std::string &name, int flags = IOFlags::OpenRead, int perms = 0666);
  ~XrdFile (void);

  virtual void	create (const char *name,
    			bool exclusive = false,
    			int perms = 0666);
  virtual void	create (const std::string &name,
    			bool exclusive = false,
    			int perms = 0666);
  virtual void	open (const char *name,
    		      int flags = IOFlags::OpenRead,
    		      int perms = 0666);
  virtual void	open (const std::string &name,
    		      int flags = IOFlags::OpenRead,
    		      int perms = 0666);

  using Storage::read;
  using Storage::readv;
  using Storage::write;
  using Storage::position;

  virtual bool		prefetch (const IOPosBuffer *what, IOSize n);
  virtual IOSize	read (void *into, IOSize n);
  virtual IOSize	read (void *into, IOSize n, IOOffset pos);
  virtual IOSize	readv (IOBuffer *into, IOSize n);
  virtual IOSize	readv (IOPosBuffer *into, IOSize n);
  virtual IOSize	write (const void *from, IOSize n);
  virtual IOSize	write (const void *from, IOSize n, IOOffset pos);

  virtual IOOffset	position (IOOffset offset, Relative whence = SET);
  virtual void		resize (IOOffset size);

  virtual void		close (void);
  virtual void		abort (void);

private:

  void                  addConnection(cms::Exception &);

  std::auto_ptr<XrdCl::File>     m_file;
  IOOffset	 	         m_offset;
  IOOffset                       m_size;
  bool			         m_close;
  std::string		         m_name;

};

#endif // XRD_ADAPTOR_XRD_FILE_H
