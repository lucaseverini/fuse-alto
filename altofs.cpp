/*******************************************************************************************
 *
 * Alto file system operations
 *
 * Copyright (c) 2016 Jürgen Buchmüller <pullmoll@t-online.de>
 *
 * This is based on L. Stewart's aar.c dated 1/18/93
 *
 *******************************************************************************************/
#include "altofs.h"

#define FIX_FREE_PAGE_BITS   0 //!< Set to 1 to fix pages marked as free in the bit_table
#define SWAP_GETPUT_WORD     msb()

AltoFS::AltoFS() :
    m_little(),
    m_kdh(),
    m_bit_count(0),
    m_bit_table(),
    m_disk_descriptor_dirty(false),
    m_sysdir(),
    m_sysdir_dirty(false),
    m_files(),
    m_disk(),
    m_doubledisk(false),
    m_dp0name(),
    m_dp1name(),
    m_verbose(0),
    m_root_dir(0)
{
    /**
     * The union's little.e is initialized to 1
     * Then little.l can be used for xor of words which need
     * a byte swap on little endian machines.
     */
    m_little.e = 1;
}

AltoFS::AltoFS(const char* filename, int verbosity, bool check, bool rebuild) :
    m_little(),
    m_kdh(),
    m_bit_count(0),
    m_bit_table(),
    m_disk_descriptor_dirty(false),
    m_sysdir(),
    m_sysdir_dirty(false),
    m_files(),
    m_disk(),
    m_doubledisk(false),
    m_dp0name(),
    m_dp1name(),
    m_verbose(verbosity),
    m_root_dir(0),
 	m_check(check),
 	m_rebuild(rebuild)
{
    /**
     * The union's little.e is initialized to 1
     * Then little.l can be used for xor of words which need
     * a byte swap on little endian machines.
     */
    m_little.e = 1;
	
    read_disk_file(filename);
	
    // verify_headers(); // Doesn't seem to be really necessary
	
    if (!validate_disk_descriptor())
	{
        fix_disk_descriptor();
	}
	
    make_fileinfo();
	
    read_sysdir();
}

AltoFS::~AltoFS()
{
    if (m_disk_descriptor_dirty)
	{
        int res = save_disk_descriptor();
        my_assert(res >= 0, "%s: Could not save the SysDir array.\n", __func__);
    }
	
    if (m_sysdir_dirty)
	{
        int res = save_sysdir();
        my_assert(res >= 0, "%s: Could not save the SysDir array.\n", __func__);
    }
	
    save_disk_file();
	
    delete m_root_dir;
	
    m_root_dir = 0;
}

void AltoFS::log(int verbosity, const char* format, ...)
{
    if (verbosity > m_verbose)
	{
        return;
	}
	
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
	
    fflush(stdout);
}

/**
 * @brief Return the current verbosity level
 * @return level (0 == silent)
 */
int AltoFS::verbosity() const
{
    return m_verbose;
}

/**
 * @brief Set the current verbosity level
 * @param verbosity level (0 == silent)
 */
void AltoFS::setVerbosity(int verbosity)
{
    m_verbose = verbosity;
}

/**
 * @brief Return a pointer to the afs_leader_t for page vda.
 * @param vda page number
 * @return pointer to afs_leader_t
 */
afs_leader_t* AltoFS::page_leader(page_t vda)
{
    afs_leader_t* lp = (afs_leader_t*)&m_disk[vda].data[0];

#if defined(DEBUG)
    if (m_verbose > 3 && lp->proplength > 0)
	{
        if (is_page_free(vda))
		{
            return lp;
		}
		
        if (lp->filename[lsb()] == 0 || lp->filename[lsb()] > FNLEN)
		{
            return lp;
		}
		
        if (lp->dir_fp_hint.blank != 0)
		{
            return lp;
		}
		
        const word offs0 = 3 * sizeof(afs_time_t) + FNLEN;
        if (lp->propbegin < offs0)
		{
            return lp;
		}
		
		static afs_leader_t empty = {{{0,}}};
		
        size_t size = lp->proplength;
        if ((lp->propbegin * + size) * sizeof(word) > offsetof(afs_leader_t, spare))
		{
            size = sizeof(lp->leader_props) / sizeof(word) - lp->propbegin;
		}
		
        if (memcmp(lp->leader_props, empty.leader_props, sizeof(lp->leader_props)))
		{
            std::string fn = filename_to_string(lp->filename);
			
            log(3, "%s: leader props for page %ld (%s)\n", __func__, vda, fn.c_str());
            log(3, "%s:   propbegin      : %u\n", __func__, lp->propbegin);
            log(3, "%s:   proplength     : %u (%u)\n", __func__, size, lp->proplength);
            log(3, "%s:   non-zero data found:\n", __func__);
			
            dump_memory((char *)lp->leader_props, size);
        }
    }
#endif
    return lp;
}

/**
 * @brief Return a pointer to the afs_label_t for page vda.
 * @param vda page number
 * @return pointer to afs_label_t
 */
afs_label_t* AltoFS::page_label(page_t vda)
{
	if (vda == 0xFFFF)
	{
		return nullptr;
	}
	
    return (afs_label_t *)&m_disk[vda].label[0];
}

/**
 * @brief Read a disk file or two of them separated by comma
 * @param name filename of the disk image(s)
 * @return 0 on succes, or -ENOSYS, -EBADF etc. on error
 */
int AltoFS::read_disk_file(std::string name)
{
    int pos = (int)name.find(',');
    if (pos > 0)
	{
        m_dp0name = std::string(name, 0, pos);
        m_dp1name = std::string(name, pos + 1);
        m_doubledisk = true;
		
		printf("Mounting double disk images:\n");
		printf("1) %s\n", m_dp0name.c_str());
		printf("2) %s\n", m_dp1name.c_str());
    }
	else
	{
        m_dp0name = name;
        m_dp1name.clear();
        m_doubledisk = false;
		
		printf("Mounting single disk image: %s\n", m_dp0name.c_str());
    }

    m_disk.resize(2 * NPAGES);
    my_assert_or_die(m_disk.data() != NULL, "%s: disk resize(%d) failed", __func__, 2 * NPAGES);

    int ok = read_single_disk(m_dp0name, &m_disk[0]);
    if (ok && m_doubledisk)
	{
        ok = read_single_disk(m_dp1name, &m_disk[NPAGES]);
    }
	
    return ok ? 0 : -ENOENT;
}

/**
 * @brief Read a single file to the in-memory disk space
 * @param name file name
 * @param diskp pointer to the starting disk page
 * @return true on success, or false on error
 */
bool AltoFS::read_single_disk(std::string name, afs_page_t* diskp)
{
    FILE *infile;
    bool ok = true;
    bool use_pclose = false;

	if(m_check)
	{
// TODO: Implement disk validity check here
	}

	if(m_rebuild)
	{
// TODO: Implement disk rebuild here
	}
	
    log(2, "%s: Reading disk image '%s'\n", __func__, name.c_str());
    // We conclude the disk image is compressed if the name ends with .Z
    int pos = (int)name.find(".Z");
    if (pos > 0)
	{
        char* cmd = new char[name.size() + 10];
        sprintf(cmd, "zcat %s", name.c_str());
        infile = popen(cmd, "r");
        my_assert_or_die(infile != NULL, "%s: popen failed on %s\n", __func__, cmd);
        delete[] cmd;
        use_pclose = true;
    }
	else
	{
        infile = fopen (name.c_str(), "rb");
        my_assert_or_die(infile != NULL, "%s: fopen failed on %s\n", __func__, name.c_str());
    }

    char *dp = reinterpret_cast<char *>(diskp);
    size_t total = NPAGES * PAGESZ;
    size_t totalbytes = 0;
    while (totalbytes < total)
	{
        size_t bytes = fread(dp, sizeof (char), total - totalbytes, infile);
        dp += bytes;
        totalbytes += bytes;
		
        ok = my_assert(!ferror(infile) && !feof(infile), "%s: Disk read failed: %d bytes read instead of %d\n", __func__, totalbytes, total);
        if (!ok)
		{
            break;
		}
    }
	
    if (use_pclose)
	{
        pclose(infile);
	}
    else
	{
        fclose(infile);
	}
	
    return ok;
}

/**
 * @brief Save the in-memory disk image(s) to a file (or two files)
 * @return 0 on success,
 */
int AltoFS::save_disk_file()
{
    bool res = save_single_disk(m_dp0name, &m_disk[0]);
    if (res && m_doubledisk)
	{
        res = save_single_disk(m_dp1name, &m_disk[NPAGES]);
	}
	
    return res;
}

/**
 * @brief Save a single disk image to a file
 * @param name name of the image file
 * @param diskp pointer to disk data
 * @return true on success, or false on error
 */
bool AltoFS::save_single_disk(std::string name, afs_page_t* diskp)
{
    FILE *outfile;
    bool ok = true;

    // We conclude the disk image is compressed if the name ends with .Z
    int pos = (int)name.find(".Z");
    if (pos > 0)
	{
        // Remove the .Z~ or .Z extension, as we will save uncompressed
        name.erase(pos);
	}
	
    // For now always write backup files
    name += "~";
    log(1, "%s: Writing disk image '%s'\n", __func__, name.c_str());

    outfile = fopen (name.c_str(), "wb");
    my_assert_or_die(outfile != NULL, "%s: fopen failed on Alto disk image file %s\n", __func__, name.c_str());

    char *dp = reinterpret_cast<char *>(diskp);
	
    size_t total = NPAGES * PAGESZ;
    size_t totalbytes = 0;
    while (totalbytes < total)
	{
        size_t bytes = fwrite(dp, sizeof (char), total - totalbytes, outfile);
        dp += bytes;
        totalbytes += bytes;
		
        ok = my_assert(!ferror(outfile), "%s: Disk write failed: %d bytes written instead of %d\n", __func__, totalbytes, total);
        if (!ok)
		{
            break;
		}
    }
	
    fclose(outfile);
	
    return ok;
}

/**
 * @brief Dump a memory block as words and ASCII data
 * @param data pointer to data to dump
 * @param nbytes number of bytes to dump
 */
void AltoFS::dump_memory(char* data, size_t nbytes)
{
    const size_t nwords = nbytes / 2;
    char str[17];

    for (size_t row = 0; row < (nwords+7)/8; row++)
	{
        log(3, "%04lx:", row * 8);
        for (size_t col = 0; col < 8; col++)
		{
            size_t offs = row * 8 + col;
            if (offs < nwords)
			{
                unsigned char h = data[(2*offs+0) ^ lsb()];
                unsigned char l = data[(2*offs+1) ^ lsb()];
                log(3, " %02x%02x", h, l);
            }
			else
			{
                log(3, "     ");
            }
        }

        for (size_t col = 0; col < 8; col++)
		{
            size_t offs = row * 8 + col;
            if (offs < nwords)
			{
                unsigned char h = data[(2*offs+0) ^ lsb()];
                unsigned char l = data[(2*offs+1) ^ lsb()];
                str[(col * 2) + 0] = (isprint(h)) ? h : '.';
                str[(col * 2) + 1] = (isprint(l)) ? l : '.';
            }
			else
			{
                str[(col * 2) + 0] = ' ';
                str[(col * 2) + 1] = ' ';
            }
        }
		
        str[16] = '\0';
        log(3, "  %16s\n", str);
    }
}

/**
 * @brief Dump a disk block as words and ASCII data
 * @param pageno page number
 */
void AltoFS::dump_disk_block(page_t pageno)
{
    char page[PAGESZ];
    read_page(pageno, page);
    dump_memory(page, PAGESZ);
}

/**
 * @brief Dump the contents of a leader page
 * @param lp pointer to afs_leader_t
 */
void AltoFS::dump_leader(afs_leader_t* lp)
{
    log(2, "%s: created                    : %s\n", __func__, altotime_to_str(lp->created).c_str());
    log(2, "%s: written                    : %s\n", __func__, altotime_to_str(lp->written).c_str());
    log(2, "%s: read                       : %s\n", __func__, altotime_to_str(lp->read).c_str());
    log(2, "%s: filename                   : %s\n", __func__, filename_to_string(lp->filename).c_str());
    log(2, "%s: leader_props[]             : ...\n", __func__);
    log(2, "%s: spare[]                    : ...\n", __func__);
    log(2, "%s: proplength                 : %u\n", __func__, lp->proplength);
    log(2, "%s: propbegin                  : %u\n", __func__, lp->propbegin);
    log(2, "%s: change_SN                  : %u\n", __func__, lp->change_SN);
    log(2, "%s: consecutive                : %u\n", __func__, lp->consecutive);
    log(2, "%s: dir_fp_hint.fid_dir        : %#x\n", __func__, lp->dir_fp_hint.fid_dir);
    log(2, "%s: dir_fp_hint.serialno       : %#x\n", __func__, lp->dir_fp_hint.serialno);
    log(2, "%s: dir_fp_hint.version        : %u\n", __func__, lp->dir_fp_hint.version);
    log(2, "%s: dir_fp_hint.blank          : %u\n", __func__, lp->dir_fp_hint.blank);
    log(2, "%s: dir_fp_hint.leader_vda     : %u\n", __func__, lp->dir_fp_hint.leader_vda);
    log(2, "%s: last_page_hint.vda         : %u\n", __func__, lp->last_page_hint.vda);
    log(2, "%s: last_page_hint.filepage    : %u\n", __func__, lp->last_page_hint.filepage);
    log(2, "%s: last_page_hint.char_pos    : %u\n", __func__, lp->last_page_hint.char_pos);
}

/**
 * @brief Return the length of a file by scanning its pages
 * @param leader_page_vda page number of the leader page
 * @return size in bytes
 */
size_t AltoFS::file_length(page_t leader_page_vda)
{
    size_t length = 0;
	
	page_t page = leader_page_vda;
    while (page != 0)
	{
        afs_label_t* l = page_label(page);
        if (l->filepage > 0)
		{
            length += l->nbytes;
		}
		
        if (l->nbytes < PAGESZ)
		{
            break;
		}
		
        page = rda_to_vda(l->next_rda);
    }
	
    return length;
}

/**
 * @brief Convert a raw disk address to a virtual disk address.
 * @param rda raw disk address (from the image)
 * @return virtual disk address (think LBA)
 */
page_t AltoFS::rda_to_vda(word rda)
{
    const word dp1flag = (rda >> 1) & 1;
    const word head = (rda >> 2) & 1;
    const word cylinder = (rda >> 3) & 0x1ff;
    const word sector = (rda >> 12) & 0xf;
    const page_t vda = (dp1flag * NPAGES) + (cylinder * NHEADS * NSECS) + (head * NSECS) + sector;
    return vda;
}

/**
 * @brief Convert a virtual disk address to a raw disk address.
 * @param vda virtual disk address (LBA)
 * @return raw disk address
 */
word AltoFS::vda_to_rda(page_t vda)
{
    const word page = vda % NPAGES;
    const word dp1flag = vda == page ? 0 : 1;
    const word cylinder = (page / (NHEADS * NSECS)) & 0x1ff;
    const word head = (page / NSECS) & 1;
    const word sector = page % NSECS;
    const word rda = (dp1flag << 1) | (head << 2) | (cylinder << 3) | (sector << 12);
    return rda;
}

/**
 * @brief Allocate a new page from the free pages.
 *
 * Scan for pages near the given page, always alternatingly looking
 * at a page after and a page before the given page.
 *
 * @param page previous page VDA where this page is chained to
 * @return new page VDA, or 0 if no free page is found
 */
page_t AltoFS::alloc_page(page_t page)
{
	log(2, "%s: prevPage=%-5ld\n", __func__, page);

	// Won't find a free page anyway
    if (m_kdh.free_pages == 0)
	{
        log(1, "%s: KDH free pages is 0 - no free page found\n", __func__);
        return 0;
    }

    const page_t maxpage = m_bit_count;
    const page_t prev_vda = page;

    afs_label_t* lprev = page ? page_label(page) : NULL;

    // Search a free page close to the current filepage
    page_t dist = 1;
    while (dist < maxpage)
	{
        if (page + dist < maxpage && !getPageBitmapBit(page + dist))
		{
            page += dist;
            break;
        }
		
        if (page - dist > 1 && !getPageBitmapBit(page - dist))
		{
            page -= dist;
            break;
        }
		
        dist++;
    }

    if (getPageBitmapBit(page))
	{
        // No free page found
        log(1, "%s: no free page found\n", __func__);
        return 0;
    }

    m_kdh.free_pages -= 1;
    m_disk_descriptor_dirty = true;
    setPageBitmapBit(page, 1);
    zero_page(page);

    afs_label_t* lthis = page_label(page);
    memset(lthis, 0, sizeof(*lthis));
    // link pages
    if (lprev)
	{
        lprev->next_rda = vda_to_rda(page);
	}
	
    lthis->prev_rda = vda_to_rda(prev_vda);
    lthis->nbytes = 0;

    if (lprev)
	{
        // copy from the previous page
        lthis->filepage = lprev->filepage + 1;
        lthis->fid_file = lprev->fid_file;
        lthis->fid_dir = lprev->fid_dir;
        lthis->fid_id = lprev->fid_id;
    }
	else
	{
        // set new values
        lthis->filepage = 0;
        lthis->fid_file = 1;
        lthis->fid_dir = 0;
        lthis->fid_id = m_kdh.last_sn.sn[lsb()];
        m_kdh.last_sn.sn[lsb()] += 1;
        lthis->nbytes = PAGESZ;
        m_disk_descriptor_dirty = true;
    }

#if defined(DEBUG)
    if (lprev)
	{
        log(2, "%s: prev page label (%ld)\n", __func__, prev_vda);
        log(2, "%s:   next_rda    : 0x%04x (vda=%ld)\n", __func__, lprev->next_rda, rda_to_vda(lprev->next_rda));
        log(2, "%s:   prev_rda    : 0x%04x (vda=%ld)\n", __func__, lprev->prev_rda, rda_to_vda(lprev->prev_rda));
        log(2, "%s:   blank       : %u\n", __func__, lprev->blank);
        log(2, "%s:   nbytes      : %u\n", __func__, lprev->nbytes);
        log(2, "%s:   filepage    : %u\n", __func__, lprev->filepage);
        log(2, "%s:   fid_file    : %#x\n", __func__, lprev->fid_file);
        log(2, "%s:   fid_dir     : %#x\n", __func__, lprev->fid_dir);
        log(2, "%s:   fid_id      : %#x\n", __func__, lprev->fid_id);
    }
	
    log(2, "%s: next page label (%ld)\n", __func__, page);
    log(2, "%s:   next_rda    : 0x%04x (vda=%ld)\n", __func__, lthis->next_rda, rda_to_vda(lthis->next_rda));
    log(2, "%s:   prev_rda    : 0x%04x (vda=%ld)\n", __func__, lthis->prev_rda, rda_to_vda(lthis->prev_rda));
    log(2, "%s:   blank       : %u\n", __func__, lthis->blank);
    log(2, "%s:   nbytes      : %u\n", __func__, lthis->nbytes);
    log(2, "%s:   filepage    : %u\n", __func__, lthis->filepage);
    log(2, "%s:   fid_file    : %#x\n", __func__, lthis->fid_file);
    log(2, "%s:   fid_dir     : %#x\n", __func__, lthis->fid_dir);
    log(2, "%s:   fid_id      : %#x\n", __func__, lthis->fid_id);
#endif
	
	log(2, "%s: page=%-5ld\n", __func__, page);

    return page;
}

/**
 * @brief Search disk for file %name and return leader page VDA.
 * @param name filename (without the trailing dot)
 * @return page number of the
 */
page_t AltoFS::find_file(const char *name)
{
    page_t page, last;
    afs_label_t* l;
    afs_leader_t* lp;

    // Use linear search !
    last = m_doubledisk ? NPAGES * 2 : NPAGES;
    for (page = 0; page < last; page++)
	{
        l = page_label(page);
        lp = page_leader(page);
        // First file page and actually a file?
        if (l->filepage == 0 && l->fid_file == 1)
		{
            std::string fn = filename_to_string(lp->filename);
            if (fn.compare(name) == 0)
			{
                return page;
			}
        }
    }
	
    my_assert(0, "%s: File %s not found\n", __func__, name);
	
    return -1;
}

/**
 * @brief Scan the SysDir file and build an array of afs_dv_t entries.
 * @return 0 on success, or -ENOENT etc. otherwise
 */
int AltoFS::read_sysdir()
{
    if (m_sysdir_dirty)
        save_sysdir();

    m_files.clear();
    afs_fileinfo* info = find_fileinfo("SysDir");
    my_assert_or_die(info != NULL, "%s: The file SysDir was not found!", __func__);
    if (info == NULL)
        return -ENOENT;

    size_t sdsize = info->statSize();
    // Allocate sysdir with slack for one extra afs_dv_t
    m_sysdir.resize(sdsize + sizeof(afs_dv_t));

    read_file(info->leader_page_vda(), m_sysdir.data(), sdsize, false);
    if (lsb())
        swabit((char *)m_sysdir.data(), sdsize);

    const afs_dv_t* end = (afs_dv_t *)(m_sysdir.data() + sdsize);
    afs_dv_t* pdv = (afs_dv_t *)m_sysdir.data();
    size_t alloc = 0;
    size_t count = 0;
    size_t deleted = 0;
    while (pdv < end)
	{
        byte type = pdv->typelength[lsb()];
        byte length = pdv->typelength[msb()];

        // End of directory if fnlen is zero (?)
        byte fnlen = pdv->filename[lsb()];
        if (fnlen == 0 || fnlen > FNLEN)
		{
            break;
		}
		
        // length is always word aligned
        size_t nsize = (fnlen | 1) + 1;
        size_t esize = sizeof(*pdv) - sizeof(pdv->filename) + nsize;
        std::string fn = filename_to_string(pdv->filename);

        // Verify filename with leader page
		afs_leader_t* lp = page_leader(pdv->fileptr.leader_vda);
		
		int idx = lsb();
		// printf("lsb: %d\n", idx);
		// printf("lp->filename: %p\n", lp->filename); // May crash here... Add pointer validation to aovid crashes to be used with scavenger
		
        byte fnlen2 = lp->filename[idx];
        log(4, "%s:* directory entry    : @%u **************\n", __func__, (word)((char *)pdv - m_sysdir.data()));
        log(4, "%s:  type               : %u (%s)\n", __func__, type, 4 == type ? "allocated" : "deleted");
        log(4, "%s:  length             : %u\n", __func__, length);
        log(4, "%s:  fileptr.fid_dir    : %#x\n", __func__, pdv->fileptr.fid_dir);
        log(4, "%s:  fileptr.serialno   : %#x\n", __func__, pdv->fileptr.serialno);
        log(4, "%s:  fileptr.version    : %#x\n", __func__, pdv->fileptr.version);
        log(4, "%s:  fileptr.blank      : %#x\n", __func__, pdv->fileptr.blank);
        log(4, "%s:  fileptr.leader_vda : %u\n", __func__, pdv->fileptr.leader_vda);
        log(4, "%s:  filename length    : %u (%u)\n", __func__, fnlen, fnlen2);
        log(4, "%s:  filename           : %s\n", __func__, fn.c_str());
		
		afs_dv dv(*pdv);
		
        if (count >= alloc)
		{
            alloc = alloc ? alloc * 2 : 32;
            m_files.reserve(alloc);
        }
		
        m_files.resize(count+1);
        m_files[count++] = dv;
		
        afs_fileinfo* info = find_fileinfo(fn);
		
        if (4 == type)
		{
            if (info)
			{
                info->setDeleted(false);
			}
        }
		else
		{
            if (info)
			{
                info->setDeleted(true);
			}
			
            deleted++;
        }
		
        pdv = (afs_dv_t*)((char *)pdv + esize);
    }

    size_t eod = (size_t)((char *)pdv - m_sysdir.data());
    log(1, "%s: SysDir usage is %u files (%u deleted) in %lu/%lu bytes\n", __func__, count, deleted, eod, sdsize);

#if defined(DEBUG)
    if (m_verbose > 4)
	{
        dump_memory(m_sysdir.data(), eod);
	}
#endif

    return 0;
}

/**
 * @brief Save an array of afs_dv_t entries to the SysDir pages.
 * @return 0 on success, or -ENOENT if SysDir is not found
 */
int AltoFS::save_sysdir()
{
    afs_fileinfo* info = find_fileinfo("SysDir");
    my_assert_or_die(info != NULL, "%s: The file SysDir was not found!", __func__);
    if (info == NULL)
        return -ENOENT;

    int res = 0;

    size_t sdsize = info->statSize();

    const afs_dv_t* end = (afs_dv_t *)(m_sysdir.data() + sdsize);
    afs_dv_t* pdv = (afs_dv_t *)m_sysdir.data();
    size_t idx = 0;
    while (idx < m_files.size() && pdv < end) {
        const afs_dv* dv = &m_files[idx];
        byte fnlen = dv->data.filename[lsb()];
        // length is always word aligned
        size_t nsize = (fnlen | 1) + 1;
        size_t esize = sizeof(*pdv) - sizeof(pdv->filename) + nsize;
        memcpy(pdv, &dv->data, esize);
        pdv = (afs_dv_t*)((char *)pdv + esize);
        idx++;
    }

    if (idx < m_files.size()) {
        // We need to increase sdsize and write the last entry
        const afs_dv* dv = &m_files[idx];
        byte fnlen = dv->data.filename[lsb()];
        // length is always word aligned
        size_t nsize = (fnlen | 1) + 1;
        size_t esize = sizeof(*pdv) - sizeof(pdv->filename) + nsize;
        size_t offs = (size_t)((char*)pdv - m_sysdir.data());
        if (offs + esize >= m_sysdir.size()) {
            // Resize m_sysdir
            m_sysdir.resize(offs + esize + 1);
            info->setStatSize(offs + esize + 1);
            m_sysdir[offs + esize] = '\0';
        }
        memcpy(pdv, &dv->data, esize);
        pdv = (afs_dv_t*)((char *)pdv + esize);
    }

    size_t eod = (size_t)((char *)pdv - m_sysdir.data());
    log(1, "%s: SysDir usage is %lu/%lu bytes\n", __func__, eod, sdsize);
    if (eod > sdsize) {
        sdsize = eod;
        m_sysdir.resize(sdsize+1);
        m_sysdir[sdsize] = '\0';
        info->setStatSize(sdsize);
    }

#if defined(DEBUG)
    if (m_verbose > 3)
        dump_memory(m_sysdir.data(), eod);
#endif
    if (lsb()) {
        std::vector<char> sysdir(m_sysdir);
        swabit(sysdir.data(), sdsize);
        size_t written = write_file(info->leader_page_vda(), sysdir.data(), eod, false);
        if (written != eod)
            res = -ENOSPC;
    } else {
        size_t written = write_file(info->leader_page_vda(), m_sysdir.data(), eod, false);
        if (written != eod)
            res = -ENOSPC;
    }
    m_sysdir_dirty = 0 == res;
    return res;
}

int AltoFS::save_disk_descriptor()
{
    int ddlp;
    afs_label_t* l;
    afs_fa_t fa;

    // Locate DiskDescriptor and copy it into the global data structure
    ddlp = (int)find_file("DiskDescriptor");
	
    my_assert_or_die(ddlp != -1, "%s: Can't find DiskDescriptor\n", __func__);

    l = page_label(ddlp);

    fa.vda = rda_to_vda(l->next_rda);
    memcpy(&m_disk[fa.vda].data[0], &m_kdh, sizeof(m_kdh));

    // Now copy the bit table from m_bit_table onto the disk
    fa.filepage = 1;
    fa.char_pos = sizeof(m_kdh);
    for (word i = 0; i < m_kdh.disk_bt_size; i++)
	{
        putword(&fa, m_bit_table[i]);
	}
	
    m_disk_descriptor_dirty = false;
	
    return 0;
}

/**
 * @brief Remove a file name in the SysDir file and write it back
 * @param name file name to look for (without trailing dot)
 * @return 0 on success, or -ENOENT on error
 */
int AltoFS::remove_sysdir_entry(std::string name)
{
    log(1, "%s: searching for '%s'\n", __func__, name.c_str());

    for (size_t idx = 0; idx < m_files.size(); idx++)
	{
        afs_dv* dv = &m_files[idx];
        std::string fn = filename_to_string(dv->data.filename);
        if (fn != name)
		{
            continue;
		}
		
        // Just mark this entry as unused
        dv->data.typelength[lsb()] = 0;
		
        log(2, "%s: found '%s' at index %ld\n", __func__, name.c_str(), idx);
		
        m_sysdir_dirty = true;
		
        return 0;
    }

    log(1, "%s: Could not find '%s' in SysDir!\n", __func__, name.c_str());
	
    return -ENOENT;
}

/**
 * @brief Rename a file name in the SysDir file and write it back
 * @param name file name to look for (without trailing dot)
 * @param newname new file name (without trailing dot)
 * @return 0 on success, or -ENOENT on error
 */
int AltoFS::rename_sysdir_entry(std::string name, std::string newname)
{
    // Never allow renaming SysDir or DiskDescriptor
    if (name.compare("SysDir") == 0 || name.compare("DiskDescriptor") == 0)
        return -EPERM;
    if (name[0] == '/')
        name.erase(0, 1);
    if (newname[0] == '/')
        newname.erase(0, 1);

    log(1, "%s: renaming '%s' to '%s'\n", __func__, name.c_str(), newname.c_str());

    // Assume failure
    int res = -ENOENT;
    for (size_t idx = 0; idx < m_files.size(); idx++) {
        afs_dv* dv = &m_files[idx];
        std::string fn = filename_to_string(dv->data.filename);
        if (fn != name)
            continue;

        // Change the name of this array entry
        string_to_filename(dv->data.filename, newname);

        fn = filename_to_string(dv->data.filename);
        log(1, "%s:  new filename       : %s.\n", __func__, fn.c_str());

        // FIXME: do we need to sort the array by name?
        m_sysdir_dirty = true;
        break;
    }

    return res;
}

/**
 * @brief Delete a file from the tree and free its chain's bits in the BT
 * @param path to the file
 * @return 0 on success, or -EPERM, -ENOMEM etc. on error
 */
int AltoFS::unlink_file(std::string path)
{
    log(2, "%s: path=%s\n", __func__, path.c_str());
    // Skip leading directory (we have only root)
    if (path[0] == '/')
	{
        path.erase(0, 1);
	}
	
    afs_fileinfo* info = find_fileinfo(path);
    if (!info)
	{
        return -ENOENT;
	}
	
    afs_leader_t* lp = page_leader(info->leader_page_vda());
    std::string fn = filename_to_string(lp->filename);

    // Never allow deleting SysDir or DiskDescriptor
    if (fn.compare("SysDir") == 0 || fn.compare("DiskDescriptor") == 0)
	{
        return -EPERM;
	}
	
    // FIXME: What needs to be zapped?
    memset(lp->filename, 0, sizeof(lp->filename));
    memset(&lp->last_page_hint, 0, sizeof(lp->last_page_hint));

    page_t page = info->leader_page_vda();
    afs_label_t* l = page_label(page);
    const word id = l->fid_id;

    // mark the file pages as unused
    while (page != 0)
	{
        l = page_label(page);
        free_page(page, id);
        if (l->nbytes < PAGESZ)
		{
            break;
		}
		
        page = rda_to_vda(l->next_rda);
    }

    // Remove this node from the file info hiearchy
    afs_fileinfo* parent = info->parent();
    if (!parent->remove(info))
	{
        log(1, "%s: Could not remove child (%p) from parent (%p).\n",
            __func__, (void*)info, (void*)parent);
    }

	log(2, "%s: parent: %p %s %d\n", __func__, parent, parent->name().c_str(), (int)parent->size());

    // Clean up the leader page label
    page = info->leader_page_vda();
    l = page_label(page);
    l->next_rda = 0;
    l->prev_rda = 0;
    l->blank = 0;
    l->fid_file = 0xffff;
    l->fid_dir = 0xffff;
    l->fid_id = 0xffff;

    return remove_sysdir_entry(fn);
}

/**
 * @brief Rename file in the tree and in SysDir
 * @param path file path
 * @param newname new filename
 * @return 0 on success, or -ENOENT on error
 */
int AltoFS::rename_file(std::string path, std::string newname)
{
    log(2, "%s: path=%s\n", __func__, path.c_str());
	
    // Skip leading directory (we have only root)
    if (path[0] == '/')
	{
        path.erase(0, 1);
	}
	
    afs_fileinfo* info = find_fileinfo(path);
    if (!info)
	{
        return -ENOENT;
	}
	
    // Skip leading directory (we have only root)
    if (newname[0] == '/')
	{
        newname.erase(0,1);
	}
	
    afs_leader_t* lp = page_leader(info->leader_page_vda());
    std::string fn = filename_to_string(lp->filename);

    int ok = my_assert(newname.length() < FNLEN-2, "%s: newname too long for '%s' -> '%s'\n", __func__, info->name().c_str(), newname.c_str());
    if (!ok)
	{
        return -EINVAL;
	}
	
    info->rename(newname);

    // Set new name in the leader page
    string_to_filename(lp->filename, newname);

    return rename_sysdir_entry(fn, newname);
}

int AltoFS::get_page_count(afs_label_t *leaderPageLabel)
{
	int pageCount = 0;
	
	page_t page = rda_to_vda(leaderPageLabel->next_rda);
	while (page != 0)
	{
		pageCount++;
		
		afs_label_t* pageLabel = page_label(page);
		page = pageLabel->next_rda != 0 ? rda_to_vda(pageLabel->next_rda) : 0;
	}
	
	return pageCount;
}

/**
 * @brief Truncate an (existing) file at the given offset
 * @param path file path
 * @param offset new size of the file
 * @return 0 on success, or -ENOENT on error
 */
int AltoFS::truncate_file(std::string path, off_t offset)
{
	int result = 0;
	
    log(2, "%s: path=%s offset=%d\n", __func__, path.c_str(), offset);
	
    // Skip leading directory (we have only root)
    if (path[0] == '/')
	{
        path.erase(0, 1);
	}
	
    afs_fileinfo* info = find_fileinfo(path);
    if (!info)
	{
        return -ENOENT;
	}
		
    afs_leader_t* lp = page_leader(info->leader_page_vda());
    afs_label_t* leaderLabel = page_label(info->leader_page_vda());
    const word id = leaderLabel->fid_id;
	
	int curPageCount = get_page_count(leaderLabel);
	int newPageCount = (int)(offset / PAGESZ);
	int lastPageSize = (int)(offset - (PAGESZ * newPageCount));
	if (lastPageSize != 0)
	{
		newPageCount++;
	}
	else
	{
		lastPageSize = offset == 0 ? 0 : PAGESZ;
		
		if (newPageCount == 0)
		{
			newPageCount = 1;
		}
	}
	
	log(2, "%s: path=%s curPageCount=%d newPageCount=%d lastPageSize=%d\n", __func__, path.c_str(), curPageCount, newPageCount, lastPageSize);
	
	afs_label_t* pageLabel = 0;
	page_t lastPage = 0;
	word lastFilePage = 1;
	off_t newOffset = 0;
	word charPos = 0;
	page_t pageToFree = 0;
	page_t page = rda_to_vda(leaderLabel->next_rda);
	while (page != 0)
	{
		pageLabel = page_label(page);
		if (pageLabel->filepage == newPageCount)
		{
			pageLabel->nbytes = lastPageSize;
			lastPage = page;
			lastFilePage = pageLabel->filepage;
		}
		else if (pageLabel->filepage < newPageCount)
		{
			pageLabel->nbytes = PAGESZ;
		}
		else if (pageLabel->filepage > newPageCount)
		{
			pageLabel->nbytes = 0;
			pageToFree = page;
		}
		
		newOffset += pageLabel->nbytes;

		page = pageLabel->next_rda != 0 ? rda_to_vda(pageLabel->next_rda) : 0;

		if(pageToFree != 0)
		{
			free_page(pageToFree, id);
			pageToFree = 0;
		}
	}
	for (int pageIdx = curPageCount; pageIdx < newPageCount; pageIdx++)
	{
		page = alloc_page(page);
		if (page == 0)
		{
			// No free page found
			info->setStatSize(static_cast<size_t>(newOffset));
			result = -ENOSPC;
			break;
		}
		
		lastPage = page;
		
		pageLabel = page_label(page);
		if(pageIdx == newPageCount)
		{
			pageLabel->nbytes = lastPageSize;
		}
		else
		{
			pageLabel->nbytes = PAGESZ;
		}
		
		lastFilePage = pageLabel->filepage;
		newOffset += pageLabel->nbytes;
	}
	
	if(lastPageSize != PAGESZ)
	{
		 charPos = lastPageSize;
	}
/*
    size_t size = info->statSize() - offset;
    page_t page = rda_to_vda(l->next_rda);
    page_t last_page = -1;
    word char_pos = 0;

    off_t offs = 0;
    while (page && size > 0)
	{
        l = page_label(page);
		
        if (offs >= offset)
		{
            // free this page
#if defined(DEBUG)
			log(3, "%s: page=%-5ld offs=%d offset=%d size=%d (free)\n", __func__, page, offs, offset, size);
#endif
            free_page(page, id);
			
            size -= l->nbytes;
            offs += PAGESZ;
            if (last_page == -1)
			{
                last_page = page;
                char_pos = 0;
            }
        }
		else if (offs + PAGESZ > offset)
		{
            // shrink this page to the remaining bytes
            l->nbytes = offset - offs;
#if defined(DEBUG)
			log(3, "%s: page=%-5ld offs=%d offset=%d size=%d (shrink to %d)\n", __func__, page, offs, offset, size, l->nbytes);
#endif
            size -= l->nbytes;
            offs += l->nbytes;
            last_page = page;
            char_pos = l->nbytes;
			
            break;  // we're done
        }
		else if (l->next_rda == 0)
		{
            // allocate a new page
            page = alloc_page(page);
            if (page == 0)
			{
                // No free page found
                info->setStatSize(static_cast<size_t>(offs));
				
                return -ENOSPC;
            }
#if defined(DEBUG)
			log(3, "%s: page=%-5ld offs=%d offset=%d size=%d (new)\n", __func__, page, offs, offset, size);
#endif
            if (offs + PAGESZ < offset)
			{
                l->nbytes = PAGESZ;
            }
			else
			{
                l->nbytes = (offset - offs) % PAGESZ;
            }
			
            last_page = page;
            char_pos = l->nbytes;
			
            // FIXME: should we fill it with zeroes?

            if (l->nbytes < PAGESZ)
			{
                break;
			}
        }
		else
		{
#if defined(DEBUG)
			log(4, "%s: page=%-5ld offs=%d offset=%d (seek)\n", __func__, page, offs, offset);
#endif
            size -= PAGESZ;
            offs += PAGESZ;
        }
		
        page = rda_to_vda(l->next_rda);
    }
 
 	lp->last_page_hint.vda = last_page;
 	lp->last_page_hint.filepage = l->filepage;
 	lp->last_page_hint.char_pos = char_pos;
*/
    lp->last_page_hint.vda = lastPage;
	lp->last_page_hint.filepage = lastFilePage;
    lp->last_page_hint.char_pos = charPos;
	
    info->setStatSize(newOffset);
	
	log(2, "%s: lastPage=%-5ld lastFilePage=%d charPos=%d newOffset=%d\n", __func__, lastPage, lastFilePage, charPos, newOffset);

    return 0;
}

/**
 * @brief Create a new file with SysDir entry, leader page and zero length first page
 * @param path filename including the path (/)
 * @return 0 on success, or -EBADF, -ENOSPC etc. otherwise
 */
int AltoFS::create_file(std::string path)
{
    log(2, "%s: path=%s\n", __func__, path.c_str());
	
    // Skip leading directory (we have only root)
    if (path[0] == '/')
	{
        path.erase(0, 1);
	}
	
    afs_fileinfo* info = find_fileinfo(path);
    if (info)
	{
        return -EEXIST;
	}
	
    // Allocate a free page as the leader page
    page_t page = alloc_page(0);

    if (!my_assert(page != 0, "%s: Found no free page\n", __func__))
	{
        return -ENOSPC;
	}

    // Get the SysDir info
    afs_leader_t* lp = page_leader(page);

    struct timeval tv;
    gettimeofday(&tv, NULL);

    // Set all times to "now"
    time_to_altotime(tv.tv_sec, &lp->created);
    time_to_altotime(tv.tv_sec, &lp->written);
    time_to_altotime(tv.tv_sec, &lp->read);

    string_to_filename(lp->filename, path);

    lp->dir_fp_hint.fid_dir = 0x8000;
    lp->dir_fp_hint.serialno = m_kdh.last_sn.sn[lsb()];
    lp->dir_fp_hint.version = 1;            // version must be 1
    lp->dir_fp_hint.blank = 0;
    // We really need the SysDir leader page number here!
    lp->dir_fp_hint.leader_vda = 1; // FIXME: m_sysdir_vda?
    lp->propbegin = offsetof(afs_leader_t, leader_props) / sizeof(word);
    lp->proplength = static_cast<byte>(sizeof(lp->leader_props) / sizeof(word));

    page_t page0 = alloc_page(page);
    my_assert(page0 != 0, "%s: Disk full when allocating first filepage of %s\n", __func__, path.c_str());
    if (page0 == 0)
	{
        return -ENOSPC;
	}
	
    // Update the last page hint
    lp->last_page_hint.vda = page0;
    lp->last_page_hint.filepage = 1;
    lp->last_page_hint.char_pos = 0;

    dump_leader(lp);

    // Find position in the SysDir array where to insert
    std::vector<afs_dv>::iterator it;
    int idx = 0;
    bool match = false;
    for (it = m_files.begin(); it != m_files.end(); it++)
	{
        afs_dv* dv = &m_files[idx];
        std::string fn = filename_to_string(dv->data.filename);
        if (fn == path && 0 == dv->data.typelength[lsb()])
		{
            match = true;
            break;
        }
		
        if (fn > path)
		{
            break;
        }
		
        idx++;
    }

    afs_dv* dv = &m_files[idx];
	
    if (!match)
	{
        if (it == m_files.end())
		{
            // Not the last entry, so make room
            log(2, "%s: insert entry at pos=%d/%ld in SysDir\n", __func__, idx, m_files.size());
            // Insert a new entry at idx
            m_files.insert(it, *dv);
        }
		else
		{
            log(2, "%s: insert entry at pos=%ld at the end of SysDir\n", __func__, m_files.size());
            m_files[idx] = *dv;
        }
    }

    dv->data.typelength[0] = path.length();                     // whatever "length" this is
    dv->data.typelength[1] = 4;                                 // this is an existing file
    dv->data.fileptr.fid_dir = 0x0000;                          // this is not a directory;
    dv->data.fileptr.serialno = m_kdh.last_sn.sn[lsb()];		// FIXME: serialno is what?
    dv->data.fileptr.version = 1;                               // The version is always == 1
    dv->data.fileptr.blank = 0x0000;                            // And blank is, well, blank
    dv->data.fileptr.leader_vda = page;                         // store the leader page
	
    string_to_filename(dv->data.filename, path);
	
	int result = make_fileinfo_file(m_root_dir, (int)page, true);
	if (result == 0)
	{
		m_sysdir_dirty = true;
	}
	
	return result;
}

int AltoFS::set_times(std::string path, const struct timespec tv[])
{
    log(2, "%s: path=%s\n", __func__, path.c_str());
	
    // Skip leading directory (we have only root)
    if (path[0] == '/')
	{
        path.erase(0, 1);
	}
	
    afs_fileinfo* info = find_fileinfo(path);
    if (!info)
	{
        return -ENOENT;
	}
	
    afs_leader_t* lp = page_leader(info->leader_page_vda());
    // FIXME: we should not be setting the ctime, but then...
    time_to_altotime(tv[1].tv_sec, &lp->created);
    // tv[0] == last access, tv[1] == last modification
    time_to_altotime(tv[1].tv_sec, &lp->written);
    time_to_altotime(tv[0].tv_sec, &lp->read);
	
    return 0;
}

int AltoFS::make_fileinfo()
{
    if (m_root_dir)
	{
        delete m_root_dir;
        m_root_dir = 0;
    }

    struct stat st;
    memset(&st, 0, sizeof(st));
    st.st_mode = S_IFDIR | 0755;
    st.st_nlink = 2;            // Set number of links = 2 for "." and ".."
    st.st_blksize = PAGESZ;
    st.st_blocks = 0;
    m_root_dir = new afs_fileinfo(0, "/", st, 0);
    my_assert(m_root_dir != 0, "%s: Allocating new root_dir failed\n", __func__);
    if (!m_root_dir)
	{
        return -ENOMEM;
	}
	
    const int last = m_doubledisk ? NPAGES * 2 : NPAGES;
    for (page_t page = 0; page < last; page++)
	{
        afs_label_t* l = page_label(page);
        // First page of a file?
        if (l->filepage != 0)
		{
            continue;
		}
        // Marked as a regular file?
        if (l->fid_file != 1)
		{
            continue;
		}
        // Previous RDA is 0?
        if (l->prev_rda != 0)
		{
            continue;
		}
		
        const int res = make_fileinfo_file(m_root_dir, (int)page, false);
        if (res < 0)
		{
            log(1, "%s: make_fileinfo_file() for page %ld failed\n", __func__, page);
            return res;
        }
    }
	
    return 0;
}

int AltoFS::make_fileinfo_file(afs_fileinfo* parent, int leader_page_vda, bool unsetDeleteFlag)
{
    afs_label_t* l = page_label(leader_page_vda);
    afs_leader_t* lp = page_leader(leader_page_vda);

    my_assert_or_die(l->filepage == 0, "%s: Page %d is not a leader page!\n", __func__, leader_page_vda);

    std::string fn = filename_to_string(lp->filename);
	
    struct stat st;
    memset(&st, 0, sizeof(st));
    st.st_ino = leader_page_vda;    // Use the leader page as inode
	
    if (l->fid_dir == 0x8000)
	{
        // A directory (SysDir) is a file which can't be modified
        st.st_mode = S_IFREG | 0400;
    }
	else if (fn.compare("DiskDescriptor") == 0)
	{
        // Don't allow DiskDescriptor to be written to
        st.st_mode = S_IFREG | 0400;
    }
	else
	{
        st.st_mode = S_IFREG | 0666;
    }
    st.st_blksize = PAGESZ;

    // Fill more struct stat info
    altotime_to_time(lp->created, &st.st_ctime);
    altotime_to_time(lp->written, &st.st_mtime);
    altotime_to_time(lp->read, &st.st_atime);
	
	if (strcmp(fn.c_str(), "backup.cm") == 0)
	{
		localtime(&st.st_ctime);
		localtime(&st.st_mtime);
		localtime(&st.st_atime);
	}
	
    st.st_nlink = 0;

    afs_fileinfo* info = new afs_fileinfo(parent, fn, st, leader_page_vda);
    my_assert(info != 0, "%s: new afs_fileinfo_t failed for %s\n", __func__, fn.c_str());
    if (!info)
	{
        return -ENOMEM;
	}
	
    // Count the file size and pages
    size_t npages = 0;
    size_t size = 0;
    while (l->next_rda != 0)
	{
        const page_t filepage = rda_to_vda(l->next_rda);
        l = page_label(filepage);
        size += l->nbytes;
        npages++;
    }
	
    info->setStatSize(size);
    info->setStatBlocks(npages);

#if defined(DEBUG)
    struct tm tm_ctime;
    altotime_to_tm(lp->created, tm_ctime);
    char ctime[40];
    strftime(ctime, sizeof(ctime), "%Y-%m-%d %H:%M:%S", &tm_ctime);
    log(3, "%s: %-40s %06o %5lu %9lu %s [%04x%04x]\n", __func__,
		info->name().c_str(), info->statMode(),
        info->statIno(), info->statSize(),
        ctime, lp->created.time[0], lp->created.time[1]);
#endif

	// The constructor set it to true
	if (unsetDeleteFlag)
	{
		info->setDeleted(false);
	}

	log(3, "%s: parent: %p %s %d\n", __func__, parent, parent->name().c_str(), (int)parent->size());

    // Make a new entry in the parent's list of children
    parent->append(info);

    return 0;
}

/**
 * @brief Get a fileinfo entry for the given path
 * @param path file name with leading path (i.e. "/" prepended)
 * @return pointer to afs_fileinfo_t for the entry, or NULL on error
 */
afs_fileinfo* AltoFS::find_fileinfo(std::string path) const
{
    if (!m_root_dir)
	{
        return NULL;
	}

    if (path == "/")
	{
        return m_root_dir;
	}
	
    if (path[0] == '/')
	{
        path.erase(0,1);
	}
	
    return m_root_dir->find(path);
}

/**
 * @brief Read the page filepage into the buffer at data
 * @param filepage page number
 * @param data buffer of PAGESZ bytes
 */
void AltoFS::read_page(page_t filepage, char* data, size_t size)
{
    const char *src = (char *)&m_disk[filepage].data;
	
    for (size_t i = 0; i < size; i++)
	{
        data[i] = src[i ^ lsb()];
	}
}

/**
 * @brief Write the page filepage to the disk image
 * @param filepage page number
 * @param data buffer of PAGESZ bytes
 */
void AltoFS::write_page(page_t filepage, const char* data, size_t size)
{
    char *dst = (char *)&m_disk[filepage].data;
	
    for (size_t i = 0; i < size; i++)
	{
        dst[i ^ lsb()] = data[i];
	}
}

/**
 * @brief Zero the page filepage in the disk image
 * @param filepage page number
 */
void AltoFS::zero_page(page_t filepage)
{
    char *dst = (char *)&m_disk[filepage].data;
    memset(dst, 0, PAGESZ);
}

/**
 * @brief Read a file starting ad leader_page_vda into the buffer at data
 * @param leader_page_vda page number of leader VDA
 * @param data buffer of size bytes
 * @param size number of bytes to read
 * @param offset start offset to read from
 * @return number of bytes actually read
 */
size_t AltoFS::read_file(page_t leader_page_vda, char* data, size_t size, off_t offset, bool update)
{
    afs_leader_t* lp = page_leader(leader_page_vda);
    afs_label_t* l = page_label(leader_page_vda);
    std::string fn = filename_to_string(lp->filename);
	
    afs_fileinfo* info = find_fileinfo(fn);
    my_assert_or_die(info != NULL, "%s: Could not find file info for %s\n", __func__, fn.c_str());
    if (info == NULL)
	{
        return -1;
	}
	
#if defined(DEBUG)
	log(3, "%s: file:%s leaderpage=%-5ld data:%p size=%d offset=%d\n", __func__, fn.c_str(), leader_page_vda, data, size, offset);
#endif
	
    off_t offs = 0;
	size_t done = 0;
	page_t page = rda_to_vda(l->next_rda); // First page with data

	char buff[PAGESZ];

	while (page && size > 0)
	{
#if defined(DEBUG)
		log(3, "%s: page=%-5ld offs=%d size=%d\n", __func__, page, offs, size);
#endif
		l = page_label(page);
        size_t nbytes = size < l->nbytes ? size : l->nbytes;
		
        if (offs >= offset)
		{
            // aligned page read
#if defined(DEBUG)
            log(3, "%s: page=%-5ld offs=%d nbytes=%d\n", __func__, page, offs, nbytes);
#endif
            read_page(page, data, nbytes);
			
            data += nbytes;
            done += nbytes;

            if (nbytes < PAGESZ)
			{
               break;
            }

			size -= nbytes;
			offs += nbytes;
        }
		else if ((off_t)(offs + PAGESZ) > offset)
		{
            // partial page read
            off_t from = offset - offs;
            nbytes -= from;
#if defined(DEBUG)
            log(3, "%s: page=%-5ld offs=%d nbytes=%d from=%d\n", __func__, page, offs, nbytes, from);
#endif
            read_page(page, buff, PAGESZ);
            memcpy(data, buff + from, nbytes);
			
            data += nbytes;
            done += nbytes;
			
            size -= nbytes;
			offs += nbytes;
        }
		else
		{
#if defined(DEBUG)
            log(3, "%s: page=%-5ld offs=%d (seeking to %d)\n", __func__, page, offs, offset);
#endif
			offs += l->nbytes;
        }
		
		page = rda_to_vda(l->next_rda);
    }

    if (update)
	{
		time_t now;
		time(&now);
		info->setStatAtime(now);

 		afs_time_t at;
		time_to_altotime(now, &at);
		lp->read = at;
	}

#if defined(DEBUG)
	log(3, "%s: file:%s done=%d\n", __func__, fn.c_str(), done);
#endif

    return done;
}

/**
 * @brief Write a file starting ad leader_page_vda from the buffer at data
 * @param leader_page_vda page number of leader VDA
 * @param data buffer of PAGESZ bytes
 * @param size number of bytes to write
 * @param offset start offset to write to
 */
size_t AltoFS::write_file(page_t leader_page_vda, const char* data, size_t size, off_t offset, bool update)
{
    afs_leader_t* lp = page_leader(leader_page_vda);
    afs_label_t* l = page_label(leader_page_vda);
    std::string fn = filename_to_string(lp->filename);
    afs_fileinfo* info = find_fileinfo(fn);
	
    my_assert_or_die(info != NULL, "%s: Could not find file info for %s\n", __func__, fn.c_str());
    if (info == NULL)
	{
        return -1;
	}
	
#if defined(DEBUG)
	log(3, "%s: file:%s leaderpage=%-5ld data:%p size=%d offset=%d\n", __func__, fn.c_str(), leader_page_vda, data, size, offset);
#endif

	off_t offs = 0;
    page_t page = rda_to_vda(l->next_rda);
	
    // See if offs is at or beyond last page
    if (offset >= (lp->last_page_hint.filepage - 1) * PAGESZ)
	{
        // In this case use the leader page's last_page_hint
        page = lp->last_page_hint.vda;
        offs = (lp->last_page_hint.filepage - 1) * PAGESZ;
    }

    size_t done = 0;
    while (page && size > 0)
	{
        l = page_label(page);
        size_t nbytes = size < PAGESZ ? size : PAGESZ;
        if (offs >= offset && l->nbytes == PAGESZ)
		{
            // aligned page write
#if defined(DEBUG)
            log(3, "%s: page=%-5ld offs=%d nbytes=%d size=%d\n", __func__, page, offs, nbytes, size);
#endif
			l->nbytes = nbytes;
			
            write_page(page, data, nbytes);
			
            data += nbytes;
            done += nbytes;
			
            size -= nbytes;
        }
		else if (l->nbytes < PAGESZ)
		{
            // partial page write to fill it up to PAGESZ
            off_t to = l->nbytes;
            nbytes = size < (size_t)(PAGESZ - to) ? size : (size_t)(PAGESZ - to);
			
            char buff[PAGESZ];
            read_page(page, buff, PAGESZ);  // get the current page
#if defined(DEBUG)
            log(3, "%s: page=%-5ld offs=%d nbytes=%d size=%d to=%d\n", __func__, page, offs, nbytes, size, to);
#endif
            memcpy(buff + to, data, nbytes);
			
            l->nbytes = to + nbytes;
			
            write_page(page, buff, l->nbytes); // write the modified page
			
            data += nbytes;
            done += nbytes;
			
            if (l->nbytes < PAGESZ)
			{
                break;
			}
			
			size -= nbytes;
        }
		else
		{
#if defined(DEBUG)
            log(3, "%s: page=%-5ld offs=%d (seeking to %d)\n", __func__, page, offs, offset);
#endif
        }
		
        offs += PAGESZ;
		
        if (size > 0 && l->next_rda == 0)
		{
            // Need to allocate a new page
            page = alloc_page(page);
        }
		else
		{
			page =
			(l->next_rda);
		}
    }

    lp->last_page_hint.vda = page;
    lp->last_page_hint.filepage = l->filepage;
    lp->last_page_hint.char_pos = l->nbytes;

    if (update)
	{
		info->setStatSize(done);

		time_t now;
		time(&now);
        info->setStatMtime(now);
		
		afs_time_t at;
		time_to_altotime(now, &at);
		lp->written = at;
    }

#if defined(DEBUG)
	log(3, "%s: file:%s done=%d created:%s written:%s read:%s\n", __func__, fn.c_str(), done,
		altotime_to_str(lp->created).c_str(), altotime_to_str(lp->written).c_str(), altotime_to_str(lp->read).c_str());
#endif

	return done;
}

/**
 * @brief convert an Alto 32-bit date/time value to *nix
 *
 * The magic offset to Unix epoch is 2117503696.
 * Adding this value seems to rely on 32bit wrap-around?
 *
 *   $ date -u --date @2117503696
 *   Fri Feb  6 03:28:16 UTC 2037
 *
 *   $ date -u --date @-2117503696
 *   Tue Nov 25 20:31:44 UTC 1902
 */
#define ALTOTIME_MAGIC 0x7E368AD0
void AltoFS::altotime_to_time(afs_time_t at, time_t* ptime)
{
    time_t time = (uint32_t)65536 * at.time[0] + at.time[1];
    if (UINT32_MAX == (uint32_t)time)
	{
        time = 1;
	}
    else
	{
        time += ALTOTIME_MAGIC;
	}
	
	// Convert to local time

    *ptime = (uint32_t)time;
}

void AltoFS::time_to_altotime(time_t time, afs_time_t* at)
{
    time -= ALTOTIME_MAGIC;
	
    at->time[0] = (uint32_t)time / 65536;
    at->time[1] = (uint32_t)time % 65536;
}

void AltoFS::altotime_to_tm(afs_time_t at, struct tm& tm)
{
    time_t time;
    altotime_to_time(at, &time);
    memcpy(&tm, localtime(&time), sizeof(tm));
}

std::string AltoFS::altotime_to_str(afs_time_t at)
{
    struct tm tm;
    altotime_to_tm(at, tm);

	char buff[64];
	snprintf(buff, sizeof(buff), "%04d-%02d-%02d %02d:%02d:%02d",
             1900 + tm.tm_year, tm.tm_mon, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);
	
    return buff;
}

word AltoFS::getword(afs_fa_t *fa)
{
    afs_label_t* l = page_label(fa->vda);
    word w;

    my_assert_or_die((fa->char_pos & 1) == 0,
        "%s: Called on odd byte boundary (%u)\n",
        __func__, fa->char_pos);

    if (fa->char_pos >= l->nbytes) {
        if (l->next_rda == 0 || l->nbytes < PAGESZ)
            return -1;
        fa->vda = rda_to_vda(l->next_rda);
        l = page_label(fa->vda);
        fa->filepage += 1;
        fa->char_pos = 0;
    }
    my_assert_or_die(fa->filepage == l->filepage,
        "%s: disk corruption - expected vda %d to be filepage %d\n",
        __func__, fa->vda, l->filepage);

    w = m_disk[fa->vda].data[fa->char_pos >> 1];
    if (SWAP_GETPUT_WORD)
        w = (w >> 8) | (w << 8);

    fa->char_pos += 2;
    return w;
}

int AltoFS::putword(afs_fa_t* fa, word w)
{
    afs_label_t* l = page_label(fa->vda);

    my_assert_or_die((fa->char_pos & 1) == 0,
        "%s: Called on odd byte boundary (%u)\n",
        __func__, fa->char_pos);

    if (fa->char_pos >= l->nbytes) {
        if (l->next_rda == 0 || l->nbytes < PAGESZ)
            return -1;
        fa->vda = rda_to_vda(l->next_rda);
        l = page_label(fa->vda);
        fa->filepage += 1;
        fa->char_pos = 0;
    }
    l->filepage = fa->filepage;

    if (SWAP_GETPUT_WORD)
        w = (w >> 8) | (w << 8);
    m_disk[fa->vda].data[fa->char_pos >> 1] = w;

    fa->char_pos += 2;
    return 0;
}

/**
 * @brief Get bit from free page bit table
 * The bit table is big endian, so page 0 is in bit 15,
 * page 1 is in bit 14, and page 15 is in bit 0
 * @param page page number
 * @return bit value 0 or 1
 */
int AltoFS::getPageBitmapBit(page_t page)
{
    if (!my_assert(page >= 0 && page < m_bit_count, "%s: page out of bounds (%d)\n", __func__, page))
	{
        return 1;
	}
	
    int offs = (int)page / 16;
    // int bit = page % 16;
    int bit = 15 - (page % 16);
	
    return (m_bit_table[offs] >> bit) & 1;
}

/**
 * @brief Set bit in the free page bit table
 * @param page page number
 * @param val value
 */
void AltoFS::setPageBitmapBit(page_t page, int val)
{
    if (!my_assert(page >= 0 && page < m_bit_count, "%s: page out of bounds (%d)\n", __func__, page))
	{
        return;
	}
	
    int offs = (int)(page / 16);
    int bit = 15 - (page % 16);
	
    if (val != ((m_bit_table[offs] >> bit) & 1))
	{
        m_bit_table[offs] = (m_bit_table[offs] & ~(1 << bit)) | ((val & 1) << bit);
        m_disk_descriptor_dirty = true;
    }
}

/**
 * @brief Free a page by filling its label fields
 *
 * Setting all three fid fields to 0xffff marks a page as unused.
 * Count up khd.free_pages, if this page was marked as in use,
 * then mark it as free.
 *
 * @param page page number
 * @param id file id
 */
void AltoFS::free_page(page_t page, word id)
{
	log(2, "%s: page:%-5ld id:0x%X\n", __func__, page, id);

    afs_label_t *l = page_label(page);

	log(2, "%s:    next_rda: 0x%X \n", __func__, l->next_rda);
	log(2, "%s:    prev_rda: 0x%X \n", __func__, l->prev_rda);
	log(2, "%s:    nbytes:   0x%X \n", __func__, l->nbytes);
	log(2, "%s:    filepage: 0x%X \n", __func__, l->filepage);
	log(2, "%s:    fid_file: 0x%X \n", __func__, l->fid_file);
	log(2, "%s:    fid_dir:  0x%X \n", __func__, l->fid_dir);
	log(2, "%s:    fid_id:   0x%X \n", __func__, l->fid_id);

	my_assert_or_die(l->nbytes == 0 || (l->nbytes > 0 && l->fid_id == id),
					 "%s: Fatal: the label id 0x%04x does not match the leader id 0x%04x\n",
					 __func__, l->fid_id, id);

	page_t prevPage = rda_to_vda(l->prev_rda);
	afs_label_t *prevPageLabel = page_label(prevPage);
	prevPageLabel->next_rda = 0;
	
	l->prev_rda = 0;
	l->nbytes = 0;
	l->filepage = -1;
	
    l->fid_file = 0xffff;
    l->fid_dir = 0xffff;
    l->fid_id = 0xffff;
	
    m_kdh.free_pages += 1;
    m_disk_descriptor_dirty = true;
	
    // mark as freed
    setPageBitmapBit(page, 0);
}

/**
 * @brief Return 1, if a page is marked as free
 * @param page page number
 * @return 1 if free, or 0 otherwise
 */
int AltoFS::is_page_free(page_t page)
{
    afs_label_t* l = page_label(page);
/*
    if (l->nbytes == 0)
	{
        return true;
	}
*/
    if (l->fid_file != 0xFFFF)
	{
        return false;
	}
	
    if (l->fid_dir != 0xFFFF)
	{
        return false;
	}
	
    if (l->fid_id != 0xFFFF)
	{
        return false;
	}
	
    return true;
}

/**
 * @brief Make sure that each page header refers to itself
 */
int AltoFS::verify_headers()
{
    int ok = 1;

    const int last = m_doubledisk ? NPAGES * 2 : NPAGES;
    for (int i = 0; i < last; i += 1)
	{
        ok &= my_assert(m_disk[i].pagenum == rda_to_vda(m_disk[i].header[1]),
            "%s: page %04x header doesn't match: %04x %04x\n",
            __func__, m_disk[i].pagenum, m_disk[i].header[0], m_disk[i].header[1]);
	}
	
    return ok;
}

/**
 * @brief Verify the disk descript file DiskDescriptor
 * Check single or double disks
 */
int AltoFS::validate_disk_descriptor()
{
    int ddlp, nfree, ok;
    afs_leader_t* lp;
    afs_label_t* l;
    afs_fa_t fa;

    // Locate DiskDescriptor and copy it into the global data structure
    ddlp = (int)find_file("DiskDescriptor");
    my_assert_or_die(ddlp != -1, "%s: Can't find DiskDescriptor\n", __func__);

    lp = page_leader(ddlp);
    // Check lp validity
	my_assert_or_die(lp != 0, "%s: Can't find page leader\n", __func__);
	my_assert_or_die(strlen(lp->filename) != 0, "%s: Invalid name in page leader\n", __func__);

    l = page_label(ddlp);
    fa.vda = rda_to_vda(l->next_rda);
    memcpy(&m_kdh, &m_disk[fa.vda].data[0], sizeof(m_kdh));
#pragma message "Is disk_bt_size a fixed value ?"
    m_bit_count = m_kdh.disk_bt_size * 16;
    m_bit_table.resize(m_kdh.disk_bt_size);

    // Now copy the bit table from the disk into bit_table
    fa.filepage = 1;
    fa.char_pos = sizeof(m_kdh);
	
    for (word i = 0; i < m_kdh.disk_bt_size; i++)
	{
        m_bit_table[i] = getword(&fa);
	}
	
    m_disk_descriptor_dirty = false;
    log(1, "%s: The bit table size is %u words (%u bits)\n", __func__, m_kdh.disk_bt_size, m_bit_count);
    ok = 1;

    if (m_doubledisk)
	{
        // For double disk systems
        ok &= my_assert(m_kdh.nDisks == 2, "%s: Expect double disk system\n", __func__);
    }
	else
	{
        // for single disk systems
        ok &= my_assert(m_kdh.nDisks == 1, "%s: Expect single disk system\n", __func__);
    }
	
    ok &= my_assert(m_kdh.nTracks == NCYLS, "%s: KDH tracks != %d\n", __func__, NCYLS);
    ok &= my_assert(m_kdh.nHeads == NHEADS, "%s: KDH heads != %d\n", __func__, NHEADS);
    ok &= my_assert(m_kdh.nSectors == NSECS, "%s: KDH sectors != %d\n", __func__, NSECS);
    ok &= my_assert(m_kdh.def_versions_kept == 0, "%s: defaultVersions != 0\n", __func__);

    // Count free pages in bit table
    nfree = 0;
    for (int i = 0; i < m_bit_count; i++)
	{
        nfree += getPageBitmapBit(i) ? 0 : 1;
	}
	
    ok &= my_assert(nfree == m_kdh.free_pages, "%s: Bit table free page count %d doesn't match KDH value %d\n", __func__, nfree, m_kdh.free_pages);

    // Count pages marked as unused in actual image
    nfree = 0;
    const page_t last = m_doubledisk ? NPAGES * 2 : NPAGES;
    for (page_t page = 0; page < last; page++)
	{
        nfree += is_page_free(page);
	}
	
	ok &= my_assert(nfree == m_kdh.free_pages, "%s: Disk image current free page count: %d doesn't match KDH value: %d\n", __func__, nfree, m_kdh.free_pages);

    return ok;
}

page_t AltoFS::scan_prev_rdas(page_t vda)
{
    afs_label_t* l = page_label(vda);
    while (l->prev_rda != 0)
	{
        vda = rda_to_vda(l->prev_rda);
        l = page_label(vda);
    }
	
    return vda;
}

/**
 * @brief Rebuild bit table and free page count from labels.
 */
void AltoFS::fix_disk_descriptor()
{
    int nfree = 0;
    int res;

//#if FIX_FREE_PAGE_BITS
    // First scan the disk image for free pages and fix up the bit table
    const page_t last = m_doubledisk ? NPAGES * 2 : NPAGES;
    for (page_t page = 0; page < last; page++)
	{
        int t = is_page_free(page);
        nfree += t;
        setPageBitmapBit(page, t == 0);
    }
//#endif

    res = make_fileinfo();
    if (res == 0)
	{
        res = read_sysdir();
	}
	
    if (res == 0)
	{
        // Reconstruct bit_table from SysDir files and their pages
        nfree = m_doubledisk ? 2 * NPAGES : NPAGES;
        for (size_t idx = 0; idx < m_files.size(); idx++)
		{
            afs_dv* file = &m_files.at(idx);
            afs_dv_t* dv = &file->data;
            byte type = dv->typelength[lsb()];
            byte fnlen = dv->filename[lsb()];
            // Skip over deleted files
            if (type != 4 || fnlen == 0)
			{
                continue;
			}
			
            page_t page = dv->fileptr.leader_vda;
            afs_leader_t* lp = page_leader(page);
            afs_label_t* l0 = page_label(page);
            size_t length = file_length(page);
            size_t pages = (length + PAGESZ - 1) / PAGESZ;
            bool fixed = false;
            word filepage = 0;
            size_t offs = 0;
			
            while (page != 0)
			{
                word nbytes = PAGESZ;
                size_t left = length - offs;
                afs_label_t* l = page_label(page);

                if (left > 0)
				{
                    if (!getPageBitmapBit(page))
					{
                        log(1, "%s: page:%-4ld filepage:%u marked as '%s' is wrong\n", __func__, page, filepage, "free");
                        fixed = true;
                    }
					
                    setPageBitmapBit(page, 1);
                    nfree--;
                }

                nbytes = l->nbytes;
                if (filepage > 0 && left >= PAGESZ && nbytes < PAGESZ)
				{
                    l->nbytes = PAGESZ;
                    log(1, "%s: page:%-4ld filepage:%u nbytes:%u is wrong (should be:%u)\n", __func__, page, filepage, nbytes, l->nbytes);
                    fixed = true;
                }

                if (filepage > 0 && left < PAGESZ && nbytes != left)
				{
                    l->nbytes = left;
                    log(1, "%s: page:%-4ld filepage:%u last page nbytes:%u is wrong (should be:%u)\n", __func__, page, filepage, nbytes, l->nbytes);
                    fixed = true;
                }

                // The following checks are only relevant for pages where nbytes > 0
                if (l->nbytes > 0)
				{
                    if (l->filepage != filepage)
					{
                        log(1, "%s: page:%-4ld filepage:%u filepage:%u is wrong (should be %u)\n", __func__, page, filepage, l->filepage, filepage);
                        l->filepage = filepage;
                        fixed = true;
                    }
                    if (l->fid_file != l0->fid_file)
					{
                        log(1, "%s: page:%-4ld filepage:%u fid_file:0x%04x is wrong (should be 0x%04x)\n", __func__, page, filepage, l->fid_file, l0->fid_file);
                        l->fid_file = l0->fid_file;
                        fixed = true;
                    }
                    if (l->fid_dir != l0->fid_dir)
					{
                        log(1, "%s: page:%-4ld filepage:%u fid_dir:0x%04x is wrong (should be 0x%04x)\n", __func__, page, filepage, l->fid_dir, l0->fid_dir);
                        l->fid_dir = l0->fid_dir;
                        fixed = true;
                    }
                    if (l->fid_id != l0->fid_id)
					{
                        log(1, "%s: page:%-4ld filepage:%u fid_id:0x%04x is wrong (should be 0x%04x)\n", __func__, page, filepage, l->fid_id, l0->fid_id);
                        l->fid_id = l0->fid_id;
                        fixed = true;
                    }
                }

                page = rda_to_vda(l->next_rda);
                if (filepage > 0)
				{
                    offs += PAGESZ;
				}
				
                filepage++;
            }
            if (fixed)
			{
                std::string fn = filename_to_string(lp->filename);
                log(1, "%s: file '%s', %ld page%s, %ld bytes was fixed\n", __func__, fn.c_str(), pages, pages != 1 ? "s" : "", length);
                if (m_verbose > 4)
				{
                    dump_leader(lp);
				}
            }
			else
			{
                std::string fn = filename_to_string(lp->filename);
                log(2, "%s: file '%s', %ld page%s, %ld bytes verified ok\n", __func__, fn.c_str(), pages, pages != 1 ? "s" : "", length);
            }
        }
    }

    // Count free pages in bit table - again
    nfree = 0;
    for (int i = 0; i < m_bit_count; i++)
	{
        nfree += getPageBitmapBit(i) ? 0 : 1;
	}
	
    my_assert (nfree == m_kdh.free_pages, "%s: Bit table free page count %d doesn't match KDH value %d\n", __func__, nfree, m_kdh.free_pages);
	
    if (m_kdh.free_pages != nfree)
	{
        m_kdh.free_pages = nfree;
        m_disk_descriptor_dirty = false;
    }
}

/**
 * @brief An assert() like function
 *
 * As opposed to assert(), this function is in debug and release
 * builds and does not break in a debug build.
 *
 * @param flag if zero, print the assert message
 * @param errmsg message format (printf style)
 * @return true if the assert is ok, false otherwise
 */
bool AltoFS::my_assert(bool flag, const char *errmsg, ...)
{
    if (flag)
        return flag;
    va_list ap;
    va_start(ap, errmsg);
    vfprintf(stdout, errmsg, ap);
    va_end(ap);
    fflush(stdout);
    return flag;
}

/**
 * @brief An assert() like function which exits
 *
 * As opposed to assert(), this function is in debug and release
 * builds. This version exits if the assertion fails.
 *
 * @param flag if false, print the assert message and exit with returncode 1
 * @param errmsg message format (printf style)
 */
void AltoFS::my_assert_or_die(bool flag, const char *errmsg, ...)
{
    if (flag)
        return;
    va_list ap;
    va_start(ap, errmsg);
    vfprintf(stdout, errmsg, ap);
    va_end(ap);
    fflush(stdout);
    exit(1);
}

/**
 * @brief Swap an array of words
 * @param data pointer to array
 * @param count number of bytes in array
 */
void AltoFS::swabit(char *data, size_t count)
{
    my_assert_or_die((count & 1) == 0 && ((size_t)data & 1) == 0,
        "%s: Called with unaligned size (%u)\n",
        __func__, count);

    my_assert_or_die(((size_t)data & 1) == 0,
        "%s: Called with unaligned data (%p)\n",
        __func__, (void*)data);

    count /= 2;
    word* d = (word *) data;
    while (count--) {
        word w = *d;
        w = (w >> 8) | (w << 8);
        *d++ = w;
    }
}

/**
 * @brief Copy an Alto file system filename from src to a C string at dst.
 *
 * In src the first byte contains the length of the string (Pascal style).
 * Every filename ends with a dot (.) which we remove here.
 *
 * @param src pointer to an Alto file system filename
 * @returns temporary pointer to the C style filename string
 */
std::string AltoFS::filename_to_string(const char* src)
{
    char buff[FNLEN+2];
    char* dst = buff;
    memset(buff, 0, sizeof(buff));

    size_t length = src[lsb()];
    if (length == 0)
	{
        return "";
	}
	
    if (length >= FNLEN)
	{
        length = FNLEN - 1;
	}
	
    for (size_t i = 0; i < length+1; i++)
	{
        dst[i] = src[i ^ lsb()];
	}
	
    // Replace invalid (non printing) characters with '#'
    for (size_t i = 1; i < length+1; i++)
	{
        dst[i] = isprint((unsigned char)dst[i]) ? dst[i] : '#';
	}
	
    // Erase a closing '.'
    my_assert('.' == buff[length], "%s: Not dot at end of filename (%s)\n", __func__, dst+1);
    if ('.' == buff[length])
	{
        buff[length] = '\0';
	}
	
    // Return the string after the length byte
    return dst+1;
}

/**
 * @brief Copy a C string from src to an Alto file system filename at dst.
 *
 * @param dst pointer to an Alto file system filename
 * @param src source buffer of at mose FNLEN-1 characters
 */
void AltoFS::string_to_filename(char* dst, std::string src)
{
    size_t length = src.length() + 1;
    if (length >= FNLEN - 2)
	{
        length = FNLEN - 2;
	}
	
    dst[lsb()] = length;
    for (size_t i = 0; i < length; i++)
	{
        dst[(i+1) ^ lsb()] = src[i];
	}
	
    // Append a dot
    dst[length ^ lsb()] = '.';
}

/**
 * @brief Fill a struct statvfs pointer with info about the file system.
 * @param vfs pointer to a struct statvfs
 * @return 0 on success, -EBADF on error (root_dir is NULL)
 */
int AltoFS::statvfs(struct statvfs* vfs)
{
    memset(vfs, 0, sizeof(*vfs));
    if (NULL == m_root_dir)
        return -EBADF;

    vfs->f_bsize = PAGESZ;              // File system block size.
    vfs->f_frsize = PAGESZ;             // Fundamental file system block size (fragment size).
	
    vfs->f_blocks = NPAGES;             // Total number of blocks on the file system, in units of f_frsize.
    if (m_doubledisk)
	{
        vfs->f_blocks *= 2;
	}
	
    vfs->f_bfree = m_kdh.free_pages;    // Total number of free blocks.
	
    vfs->f_bavail = m_kdh.free_pages;   // Total number of free blocks available to non-privileged processes.
	
    vfs->f_files = (int)m_files.size(); // Total number of file nodes (inodes) on the file system.
	
    // Per 2 free pages we could create 1 file (leader page and 1st file page)
    size_t inodes = m_kdh.free_pages / 2;
	vfs->f_ffree = (fsfilcnt_t)inodes;  // Total number of free file nodes (inodes).
    vfs->f_favail = (fsfilcnt_t)inodes; // Total number of free file nodes (inodes) available to non-privileged processes.
	
	// File system ID number. Use the "serialno" stored in the disk header
    vfs->f_fsid = m_kdh.last_sn.sn[lsb()];
	
    // Flags: can't set uid, no devices, no executable files
#ifdef __APPLE__
	vfs->f_flag = ST_NOSUID;
#else
	vfs->f_flag = ST_NOSUID | ST_NODEV | ST_NOEXEC;
#endif
    vfs->f_namemax = FNLEN-2;
	
    return 0;
}

void AltoFS::print_file_pages(page_t leader_page_vda)
{
	log(1, "#### print_file_pages ####\n");
	
	page_t page = leader_page_vda;
	while (page != 0)
	{
		log(1, "%s : %ld\n", page == leader_page_vda ? "leader_page" : "page", page);

		afs_label_t* pageLabel = page_label(page);

		log(1, "    next_rda : %d  vda=%ld\n", pageLabel->next_rda, rda_to_vda(pageLabel->next_rda));
		log(1, "    prev_rda : %d  vda=%ld\n", pageLabel->prev_rda, rda_to_vda(pageLabel->prev_rda));
		log(1, "    blank    : %d\n", pageLabel->blank);
		log(1, "    nbytes   : %d\n", pageLabel->nbytes);
		log(1, "    filepage : %d\n", pageLabel->filepage);
		log(1, "    fid_file : %#x\n", pageLabel->fid_file);
		log(1, "    fid_dir  : %#x\n", pageLabel->fid_dir);
		log(1, "    fid_id   : %#x\n", pageLabel->fid_id);
		
		page = rda_to_vda(pageLabel->next_rda);
	}
	
	log(1, "-------------------------\n");
}

