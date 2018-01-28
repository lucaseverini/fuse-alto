#if !defined(_FILEINFO_H_)
#define _FILEINFO_H_

#include <sys/stat.h>
#include <cstddef>
#include <string>
#include <vector>

#include "afs_types.h"

/**
 * @brief Class to keep information about a file or directory
 */
class afs_fileinfo
{
public:
    afs_fileinfo();
    afs_fileinfo(afs_fileinfo* parent, std::string name, struct stat st, int vda, bool deleted = true);
    ~afs_fileinfo();

    afs_fileinfo* parent() const;
    std::string name() const;
    struct stat* st();
    const struct stat* st() const;
    page_t leader_page_vda() const;
    bool deleted() const;
    void setDeleted(bool on);
    int size() const;
    std::vector<afs_fileinfo*> children() const;
    afs_fileinfo* child(int idx);
    const afs_fileinfo* child(int idx) const;

    afs_fileinfo* find(std::string name);

    ino_t statIno() const;
    time_t statCtime() const;
    time_t statMtime() const;
    time_t statAtime() const;
    uid_t statUid() const;
    gid_t statGid() const;
    mode_t statMode() const;
    size_t statSize() const;
    size_t statBlockSize() const;
    size_t statBlocks() const;
    size_t statNLink() const;

    void setIno(ino_t ino);
    void setStatCtime(time_t t);
    void setStatMtime(time_t t);
    void setStatAtime(time_t t);
    void setStatUid(uid_t uid);
    void setStatGid(gid_t gid);
    void setStatMode(mode_t mode);
    void setStatSize(size_t size);
    void setStatBlockSize(size_t blocksize);
    void setStatBlocks(size_t blocks);
    void setStatNLink(size_t count);

    void erase(int pos, int count = 1);
    void erase(std::vector<afs_fileinfo*>::iterator pos);
    void rename(std::string newname);
    void append(afs_fileinfo* child);
    bool remove(afs_fileinfo* child);

private:
    afs_fileinfo* m_parent;                 //!< Parent directory
    std::string m_name;                     //!< Filename
    struct stat m_st;                       //!< Status
    page_t m_leader_page_vda;               //!< Leader page of this file
    bool m_deleted;                         //!< True, if the file is marked as deleted
    std::vector<afs_fileinfo*> m_children;  //!< Vector of child nodes
};

#endif // !defined(_FILEINFO_H_)
