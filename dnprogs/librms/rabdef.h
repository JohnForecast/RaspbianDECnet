/* <rabdef.h>
 *
 *	RMS Record Access Block (rab) definitions
 */
#ifndef _RABDEF_H
#define _RABDEF_H

/*
   There is one rab per connected stream it is used for all communications
   between the user and rms concerning operations on the stream.
 */
#define RAB$C_BID	1	/* code for rab */
/* special ISI fields for process permanent files */
#define RAB$V_PPF_RAT	6
#define RAB$M_PPF_RAT	0x3FC0
#define RAB$S_PPF_RAT	8
#define RAB$V_PPF_IND	14
#define RAB$M_PPF_IND	0x4000
#define RAB$V_PPISI	15
#define RAB$M_PPISI	0x8000
/* record options (ROP) mask bits */
#define RAB$V_ASY	0
#define RAB$V_TPT	1
#define RAB$V_REA	2
#define RAB$V_RRL	3
#define RAB$V_UIF	4
#define RAB$V_MAS	5
#define RAB$V_FDL	6
#ifndef NO_VMS_V6
#define RAB$V_REV	7
#endif
#ifndef ONLY_VMS_V6
#define RAB$V_HSH	7
#endif
#define RAB$V_EOF	8
#define RAB$V_RAH	9
#define RAB$V_WBH	10
#define RAB$V_BIO	11
#define RAB$V_CDK	12
#define RAB$V_LV2	12
#define RAB$V_LOA	13
#define RAB$V_LIM	14
#define RAB$V_SYNCSTS	15
#define RAB$V_LOC	16
#define RAB$V_WAT	17
#define RAB$V_ULK	18
#define RAB$V_RLK	19
#define RAB$V_NLK	20
#define RAB$V_KGE	21
#define RAB$V_EQNXT	RAB$V_KGE	/* 21 */
#define RAB$V_KGT	22
#define RAB$V_NXT	RAB$V_KGT	/* 22 */
#define RAB$V_NXR	23
#define RAB$V_RNE	24
#define RAB$V_TMO	25
#define RAB$V_CVT	26
#define RAB$V_RNF	27
#define RAB$V_ETO	28
#define RAB$V_PTA	29
#define RAB$V_PMT	30
#define RAB$V_CCO	31
#define RAB$M_ASY	(1<<RAB$V_ASY)		/* 0x01 */
#define RAB$M_TPT	(1<<RAB$V_TPT)		/* 0x02 */
#define RAB$M_REA	(1<<RAB$V_REA)		/* 0x04 */
#define RAB$M_RRL	(1<<RAB$V_RRL)		/* 0x08 */
#define RAB$M_UIF	(1<<RAB$V_UIF)		/* 0x10 */
#define RAB$M_MAS	(1<<RAB$V_MAS)		/* 0x20 */
#define RAB$M_FDL	(1<<RAB$V_FDL)		/* 0x40 */
#ifndef NO_VMS_V6
#define RAB$M_REV	(1<<RAB$V_REV)		/* 0x80 */
#endif
#ifndef ONLY_VMS_V6
#define RAB$M_HSH	(1<<RAB$V_HSH)		/* 0x80 */
#endif
#define RAB$M_EOF	(1<<RAB$V_EOF)		/* 0x0100 */
#define RAB$M_RAH	(1<<RAB$V_RAH)		/* 0x0200 */
#define RAB$M_WBH	(1<<RAB$V_WBH)		/* 0x0400 */
#define RAB$M_BIO	(1<<RAB$V_BIO)		/* 0x0800 */
#define RAB$M_CDK	(1<<RAB$V_CDK)		/* 0x1000 */
#define RAB$M_LV2	(1<<RAB$V_LV2)		/* 0x1000 */
#define RAB$M_LOA	(1<<RAB$V_LOA)		/* 0x2000 */
#define RAB$M_LIM	(1<<RAB$V_LIM)		/* 0x4000 */
#define RAB$M_SYNCSTS	(1<<RAB$V_SYNCSTS)	/* 0x8000 */
#define RAB$M_LOC	(1<<RAB$V_LOC)		/* 0x010000 */
#define RAB$M_WAT	(1<<RAB$V_WAT)		/* 0x020000 */
#define RAB$M_ULK	(1<<RAB$V_ULK)		/* 0x040000 */
#define RAB$M_RLK	(1<<RAB$V_RLK)		/* 0x080000 */
#define RAB$M_NLK	(1<<RAB$V_NLK)		/* 0x100000 */
#define RAB$M_KGE	(1<<RAB$V_KGE)		/* 0x200000 */
#define RAB$M_EQNXT	(1<<RAB$V_EQNXT)	/* 0x00200000 */
#define RAB$M_KGT	(1<<RAB$V_KGT)		/* 0x400000 */
#define RAB$M_NXT	(1<<RAB$V_NXT)		/* 0x00400000 */
#define RAB$M_NXR	(1<<RAB$V_NXR)		/* 0x800000 */
#define RAB$M_RNE	(1<<RAB$V_RNE)		/* 0x01000000 */
#define RAB$M_TMO	(1<<RAB$V_TMO)		/* 0x02000000 */
#define RAB$M_CVT	(1<<RAB$V_CVT)		/* 0x04000000 */
#define RAB$M_RNF	(1<<RAB$V_RNF)		/* 0x08000000 */
#define RAB$M_ETO	(1<<RAB$V_ETO)		/* 0x10000000 */
#define RAB$M_PTA	(1<<RAB$V_PTA)		/* 0x20000000 */
#define RAB$M_PMT	(1<<RAB$V_PMT)		/* 0x40000000 */
#define RAB$M_CCO	(1<<RAB$V_CCO)		/* 0x80000000 */
/* record access (RAC) codes */
#define RAB$C_SEQ	0	/* sequential access */
#define RAB$C_KEY	1	/* keyed access */
#define RAB$C_RFA	2	/* rfa access */
#define RAB$C_STM	3	/* stream access (valid only for sequential org) */
#define RAB$C_MAXRAC	2	/* maximum RAC value currently supported by RMS [sic] */

#define RAB$K_BLN	68	/* length of rab */
#define RAB$C_BLN	68

struct RAB {
    unsigned char  rab$b_bid;		/* block id */
    unsigned char  rab$b_bln;		/* block length */
    unsigned short rab$w_isi;		/* internal stream index */
    unsigned long  rab$l_rop;		/* record options */
    unsigned long  rab$l_sts;		/* status */
    unsigned long  rab$l_stv;		/* status value */
    unsigned short rab$w_rfa[3];	/* record's file address */
    unsigned		: 16;		/* (reserved - used for optimized MOVQ) */
    unsigned long  rab$l_ctx;		/* user context */
    unsigned		: 16;		/* (spare) */
    unsigned char  rab$b_rac;		/* record access */
    unsigned char  rab$b_tmo;		/* time-out period */
    unsigned short rab$w_usz;		/* user buffer size */
    unsigned short rab$w_rsz;		/* record buffer size */
    void	  *rab$l_ubf;		/* user buffer address */
    void	  *rab$l_rbf;		/* record buffer address */
    void	  *rab$l_rhb;		/* record header buffer addr */
    void	  *rab$l_kbf;		/* key buffer address */
    unsigned char  rab$b_ksz;		/* key buffer size */
    unsigned char  rab$b_krf;		/* key of reference */
    char	   rab$b_mbf;		/* multi-buffer count */
    unsigned char  rab$b_mbc;		/* multi-block count */
    unsigned long  rab$l_bkt;		/* bucket hash code, vbn, or rrn */
    void	  *rab$l_fab;		/* related fab for connect */
    void	  *rab$l_xab;		/* XAB address */
};

struct rabdef {
    unsigned char rab$b_bid;		/* block id */
    unsigned char rab$b_bln;		/* block length */
    union {
	unsigned short rab$w_isi;	/* internal stream index */
	struct {
	    unsigned		    : 6; /* move to bit 6 */
	    unsigned rab$v_ppf_rat  : 8; /* rat value for process-permanent files */
	    unsigned rab$v_ppf_ind  : 1; /* indirect access to process-permanent file */
	    unsigned rab$v_ppisi    : 1; /* indicates that this is process-permanent stream */
	} rab$r_isi_bits;
    } rab$r_isi_overlay;
    union {
	unsigned long rab$l_rop;	/* record options */
	struct {
	    unsigned rab$v_asy : 1;	/* asynchronous operations */
	    unsigned rab$v_tpt : 1;	/* truncate put - allow sequential put not at */
					/* eof, thus truncating file (seq. org only) */
/*
   These next two should be in the byte for bits
   input to $find or $get, but there is no room there.
 */
	    unsigned rab$v_rea : 1;	/* lock record for read only, allow other readers */
	    unsigned rab$v_rrl : 1;	/* read record regardless of lock */

	    unsigned rab$v_uif : 1;	/* update if existent */
	    unsigned rab$v_mas : 1;	/* mass-insert mode */
	    unsigned rab$v_fdl : 1;	/* fast record deletion */
#ifndef NO_VMS_V6
	    unsigned rab$v_rev : 1;	/* reverse-search - can only be set with NXT or EQNXT */
#ifndef ONLY_VMS_V6
#define  rab$v_hsh rab$v_rev
#endif
#else
	    unsigned rab$v_hsh : 1;	/* use hash code in bkt */
#endif

	    unsigned rab$v_eof : 1;	/* connect to eof */
	    unsigned rab$v_rah : 1;	/* read ahead */
	    unsigned rab$v_wbh : 1;	/* write behind */
	    unsigned rab$v_bio : 1;	/* connect for bio only */
	    unsigned rab$v_cdk : 1;	/* check for duplicate keys on $GET */
	    unsigned rab$v_loa : 1;	/* use bucket fill percentage */
	    unsigned rab$v_lim : 1;	/* compare for key limit reached on $get/$find seq. (idx only) */
	    unsigned rab$v_syncsts : 1; /* Synchronous status notification for asynchronous routines. */
/*
   The following bits are input to $find or $get, (see above also
   REA and RRL).  (separate byte)
 */
	    unsigned rab$v_loc : 1;	/* use locate mode */
	    unsigned rab$v_wat : 1;	/* wait if record not available */
	    unsigned rab$v_ulk : 1;	/* manual unlocking */
	    unsigned rab$v_rlk : 1;	/* allow readers for this locked record */
	    unsigned rab$v_nlk : 1;	/* do not lock record */
	    unsigned rab$v_kge : 1;	/* key > or = */
	    unsigned rab$v_kgt : 1;	/* key greater than */
	    unsigned rab$v_nxr : 1;	/* get non-existent record */
/*
    The following bits are terminal qualifiers only.  (separate byte)
 */
	    unsigned rab$v_rne : 1;	/* read no echo */
	    unsigned rab$v_tmo : 1;	/* use time-out period */
	    unsigned rab$v_cvt : 1;	/* convert to upper case */
	    unsigned rab$v_rnf : 1;	/* read no filter */
	    unsigned rab$v_eto : 1;	/* extended terminal operation */
	    unsigned rab$v_pta : 1;	/* purge type ahead */
	    unsigned rab$v_pmt : 1;	/* use prompt buffer */
	    unsigned rab$v_cco : 1;	/* cancel control o on output */
	} rab$r_rop_bits0;
	struct {
	    unsigned		 : 21;
	    unsigned rab$v_eqnxt : 1;	/* synonyms for KGE and */
	    unsigned rab$v_nxt	 : 1;	/*  KGT */
	    unsigned		 : 1;
	} rab$r_rop_bits1;
	struct {
	    unsigned	: 8;		/* char fill; */
	    unsigned char rab$b_rop1;	/* various options */
	    unsigned char rab$b_rop2;	/* get/find options (use of this field discouraged */
					/* due to REA and RRL being in a different byte) */
	    unsigned char rab$b_rop3;	/* terminal read options */
	} rab$r_rop_fields;
    } rab$r_rop_overlay;
    unsigned long rab$l_sts;		/* status */
    union {
	unsigned long rab$l_stv;	/* status value */
	struct {
	    unsigned short rab$w_stv0;	/* low word of stv */
	    unsigned short rab$w_stv2;	/* high word of stv */
	} rab$r_stv_fields;
    } rab$r_stv_overlay;
    union {
	unsigned short rab$w_rfa[3];	/* record's file address */
	struct {
	    unsigned long rab$l_rfa0;
	    unsigned short rab$w_rfa4;
	} rab$r_rfa_fields;
    } rab$r_rfa_overlay;
    unsigned	: 16;		/* short fill; (reserved - rms release 1 optimizes stores to the */
				/*  rfa field to be a move quad, overwriting this reserved word) */
    unsigned long rab$l_ctx;		/* user context */
    unsigned	: 16;			/* short fill; (spare) */
    unsigned char rab$b_rac;		/* record access */
    unsigned char rab$b_tmo;		/* time-out period */
    unsigned short rab$w_usz;		/* user buffer size */
    unsigned short rab$w_rsz;		/* record buffer size */
    unsigned long rab$l_ubf;		/* user buffer address */
    unsigned long rab$l_rbf;		/* record buffer address */
    unsigned long rab$l_rhb;		/* record header buffer addr */
    union {
	unsigned long rab$l_kbf;	/* key buffer address */
	unsigned long rab$l_pbf;	/* prompt buffer addr */
#define rab$l_pbf rab$l_kbf
    } rab$r_kbf_overlay;
    union {
	unsigned char rab$b_ksz;	/* key buffer size */
	unsigned char rab$b_psz;	/* prompt buffer size */
#define rab$l_psz rab$l_ksz
    } rab$r_ksz_overlay;
    unsigned char rab$b_krf;		/* key of reference */
    char rab$b_mbf;			/* multi-buffer count */
    unsigned char rab$b_mbc;		/* multi-block count */
    union {
	unsigned long rab$l_bkt;	/* bucket hash code, vbn, or rrn */
	unsigned long rab$l_dct;	/* duplicates count on key accessed on alternate key */
#define rab$l_dct rab$l_bkt
    } rab$r_bkt_overlay;
    unsigned long rab$l_fab;		/* related fab for connect */
    unsigned long rab$l_xab;		/* XAB address */
};

/* declare initialized prototype data structure */
extern struct RAB cc$rms_rab __asm("_$$PsectAttributes_GLOBALSYMBOL$$cc$rms_rab");
/* globalref struct RAB cc$rms_rab; */

#endif	/*_RABDEF_H*/
