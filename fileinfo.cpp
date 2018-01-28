#include "fileinfo.h"

afs_fileinfo::afs_fileinfo() :
    m_parent(0),
    m_name(),
    m_st(),
    m_leader_page_vda(0),
    m_deleted(true),
    m_children()
{
}

afs_fileinfo::afs_fileinfo(afs_fileinfo* parent, std::string name, struct stat st, int vda, bool deleted) :
    m_parent(parent),
    m_name(name),
    m_st(st),
    m_leader_page_vda(vda),
    m_deleted(deleted),
    m_children()
{
}

afs_fileinfo::~afs_fileinfo()
{
    std::vector<afs_fileinfo*>::iterator it = m_children.begin();
    while (it != m_children.end())
	{
        afs_fileinfo* child = m_children.at(0);
        m_children.erase(it);
        delete child;
    }
}

afs_fileinfo* afs_fileinfo::parent() const
{
    return m_parent;
}

std::string afs_fileinfo::name() const
{
    return m_name;
}

struct stat* afs_fileinfo::st()
{
    return &m_st;
}

const struct stat* afs_fileinfo::st() const
{
    return &m_st;
}

page_t afs_fileinfo::leader_page_vda() const
{
    return m_leader_page_vda;
}

bool afs_fileinfo::deleted() const
{
    return m_deleted;
}

void afs_fileinfo::setDeleted(bool on)
{
    m_deleted = on;
}

int afs_fileinfo::size() const
{
    return (int)m_children.size();
}

std::vector<afs_fileinfo*> afs_fileinfo::children() const
{
    return m_children;
}

afs_fileinfo* afs_fileinfo::child(int idx)
{
    return m_children.at(idx);
}

const afs_fileinfo* afs_fileinfo::child(int idx) const
{
    return m_children.at(idx);
}

afs_fileinfo* afs_fileinfo::find(std::string name)
{
    std::vector<afs_fileinfo*>::iterator it;
    for (it = m_children.begin(); it != m_children.end(); it++) {
        afs_fileinfo* node = *it.base();
        if (!node)
            continue;
        if (name == node->name())
            return node;
    }
    return NULL;

}

ino_t afs_fileinfo::statIno() const
{
    return m_st.st_ino;
}

time_t afs_fileinfo::statCtime() const
{
    return m_st.st_ctime;
}

time_t afs_fileinfo::statMtime() const
{
    return m_st.st_mtime;
}

time_t afs_fileinfo::statAtime() const
{
    return m_st.st_atime;
}

uid_t afs_fileinfo::statUid() const
{
    return m_st.st_uid;
}

gid_t afs_fileinfo::statGid() const
{
    return m_st.st_gid;
}

mode_t afs_fileinfo::statMode() const
{
    return m_st.st_mode;
}

size_t afs_fileinfo::statSize() const
{
    return m_st.st_size;
}

size_t afs_fileinfo::statBlockSize() const
{
    return m_st.st_blksize;
}

size_t afs_fileinfo::statBlocks() const
{
    return m_st.st_blocks;
}

size_t afs_fileinfo::statNLink() const
{
    return m_st.st_nlink;
}

void afs_fileinfo::setIno(ino_t ino)
{
    m_st.st_ino = ino;
}

void afs_fileinfo::setStatCtime(time_t t)
{
    m_st.st_ctime = t;
}

void afs_fileinfo::setStatMtime(time_t t)
{
    m_st.st_mtime = t;
}

void afs_fileinfo::setStatAtime(time_t t)
{
    m_st.st_atime = t;
}

void afs_fileinfo::setStatUid(uid_t uid)
{
    m_st.st_uid = uid;
}

void afs_fileinfo::setStatGid(gid_t gid)
{
    m_st.st_gid = gid;
}

void afs_fileinfo::setStatMode(mode_t mode)
{
    m_st.st_mode = mode;
}

void afs_fileinfo::setStatSize(size_t size)
{
    m_st.st_size = size;
}

void afs_fileinfo::setStatBlockSize(size_t blocksize)
{
    m_st.st_blksize = (int)blocksize;
}

void afs_fileinfo::setStatBlocks(size_t blocks)
{
    m_st.st_blocks = blocks;
}

void afs_fileinfo::setStatNLink(size_t count)
{
    m_st.st_nlink = count;
}

void afs_fileinfo::erase(int pos, int count)
{
    std::vector<afs_fileinfo*>::iterator it;
    int idx = 0;
    for (it = m_children.begin(); it != m_children.end(); it++)
	{
        if (idx < pos)
            continue;
		
        if (idx >= pos + count)
            break;
		
        m_children.erase(it);
        m_st.st_nlink -= 1;
		
        idx++;
    }
}

void afs_fileinfo::erase(std::vector<afs_fileinfo*>::iterator pos)
{
    m_children.erase(pos);
}

void afs_fileinfo::rename(std::string newname)
{
    m_name = newname;
}

void afs_fileinfo::append(afs_fileinfo* info)
{
    m_children.push_back(info);
    m_st.st_nlink += 1;
}

bool afs_fileinfo::remove(afs_fileinfo* child)
{
    std::vector<afs_fileinfo*>::iterator it;
    int idx = 0;
    for (it = m_children.begin(); it != m_children.end(); it++)
	{
        afs_fileinfo* node = m_children.at(idx);
        if (child->name() == node->name())
		{
            m_children.erase(it);
            return true;
        }
		
        idx++;
    }
	
    return false;
}
