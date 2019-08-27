/* <fabdef.h>
 *
 *	RMS File Access Block definitions
 */
#ifndef _FABDEF_H
#define _FABDEF_H

#define FAB$C_BID	3	/* code for fab */
/* special IFI fields for process permanent files */
#define FAB$V_PPF_RAT	6
#define FAB$M_PPF_RAT	0x3FC0
#define FAB$S_PPF_RAT	8
#define FAB$V_PPF_IND	14
#define FAB$M_PPF_IND	0x4000
#define FAB$V_PPIFI	15
#define FAB$M_PPIFI	0x8000
/* file options (FOP) mask bits */
#define FAB$V_ASY	0
#define FAB$V_MXV	1
#define FAB$V_SUP	2
#define FAB$V_TMP	3
#define FAB$V_TMD	4
#define FAB$V_DFW	5
#define FAB$V_SQO	6
#define FAB$V_RWO	7
#define FAB$V_POS	8
#define FAB$V_WCK	9
#define FAB$V_NEF	10
#define FAB$V_RWC	11
#define FAB$V_DMO	12
#define FAB$V_SPL	13
#define FAB$V_SCF	14
#define FAB$V_DLT	15
#define FAB$V_NFS	16
#define FAB$V_UFO	17
#define FAB$V_PPF	18
#define FAB$V_INP	19
#define FAB$V_CTG	20
#define FAB$V_CBT	21
#define FAB$V_SYNCSTS	22
#define FAB$V_JNL	22	/* obsolete */
#define FAB$V_RCK	23
#define FAB$V_NAM	24
#define FAB$V_CIF	25
#define FAB$V_UFM	26	/* obsolete */
#define FAB$V_ESC	27
#define FAB$V_TEF	28
#define FAB$V_OFP	29
#define FAB$V_KFO	30
#define FAB$M_ASY	(1<<FAB$V_ASY)		/* 0x01 */
#define FAB$M_MXV	(1<<FAB$V_MXV)		/* 0x02 */
#define FAB$M_SUP	(1<<FAB$V_SUP)		/* 0x04 */
#define FAB$M_TMP	(1<<FAB$V_TMP)		/* 0x08 */
#define FAB$M_TMD	(1<<FAB$V_TMD)		/* 0x10 */
#define FAB$M_DFW	(1<<FAB$V_DFW)		/* 0x20 */
#define FAB$M_SQO	(1<<FAB$V_SQO)		/* 0x40 */
#define FAB$M_RWO	(1<<FAB$V_RWO)		/* 0x80 */
#define FAB$M_POS	(1<<FAB$V_POS)		/* 0x0100 */
#define FAB$M_WCK	(1<<FAB$V_WCK)		/* 0x0200 */
#define FAB$M_NEF	(1<<FAB$V_NEF)		/* 0x0400 */
#define FAB$M_RWC	(1<<FAB$V_RWC)		/* 0x0800 */
#define FAB$M_DMO	(1<<FAB$V_DMO)		/* 0x1000 */
#define FAB$M_SPL	(1<<FAB$V_SPL)		/* 0x2000 */
#define FAB$M_SCF	(1<<FAB$V_SCF)		/* 0x4000 */
#define FAB$M_DLT	(1<<FAB$V_DLT)		/* 0x8000 */
#define FAB$M_NFS	(1<<FAB$V_NFS)		/* 0x010000 */
#define FAB$M_UFO	(1<<FAB$V_UFO)		/* 0x020000 */
#define FAB$M_PPF	(1<<FAB$V_PPF)		/* 0x040000 */
#define FAB$M_INP	(1<<FAB$V_INP)		/* 0x080000 */
#define FAB$M_CTG	(1<<FAB$V_CTG)		/* 0x100000 */
#define FAB$M_CBT	(1<<FAB$V_CBT)		/* 0x200000 */
#define FAB$M_SYNCSTS	(1<<FAB$V_SYNCSTS)	/* 0x400000 */
#define FAB$M_JNL	(1<<FAB$V_JNL)		/*(0x400000)*/
#define FAB$M_RCK	(1<<FAB$V_RCK)		/* 0x800000 */
#define FAB$M_NAM	(1<<FAB$V_NAM)		/* 0x01000000 */
#define FAB$M_CIF	(1<<FAB$V_CIF)		/* 0x02000000 */
#define FAB$M_UFM	(1<<FAB$V_UFM)		/* 0x04000000 */
#define FAB$M_ESC	(1<<FAB$V_ESC)		/* 0x08000000 */
#define FAB$M_TEF	(1<<FAB$V_TEF)		/* 0x10000000 */
#define FAB$M_OFP	(1<<FAB$V_OFP)		/* 0x20000000 */
#define FAB$M_KFO	(1<<FAB$V_KFO)		/* 0x40000000 */
/* file access (FAC) mask bits */
#define FAB$V_PUT	0
#define FAB$V_GET	1
#define FAB$V_DEL	2
#define FAB$V_UPD	3
#define FAB$V_TRN	4
#define FAB$V_BIO	5
#define FAB$V_BRO	6
#define FAB$V_EXE	7
#define FAB$M_PUT	(1<<FAB$V_PUT)		/* 0x01 */
#define FAB$M_GET	(1<<FAB$V_GET)		/* 0x02 */
#define FAB$M_DEL	(1<<FAB$V_DEL)		/* 0x04 */
#define FAB$M_UPD	(1<<FAB$V_UPD)		/* 0x08 */
#define FAB$M_TRN	(1<<FAB$V_TRN)		/* 0x10 */
#define FAB$M_BIO	(1<<FAB$V_BIO)		/* 0x20 */
#define FAB$M_BRO	(1<<FAB$V_BRO)		/* 0x40 */
#define FAB$M_EXE	(1<<FAB$V_EXE)		/* 0x80 */
/* file sharing (SHR) mask bits */
#define FAB$V_SHRPUT	0
#define FAB$V_SHRGET	1
#define FAB$V_SHRDEL	2
#define FAB$V_SHRUPD	3
#define FAB$V_MSE	4
#define FAB$V_NIL	5
#define FAB$V_UPI	6
#define FAB$M_SHRPUT	(1<<FAB$V_SHRPUT)	/* 0x01 */
#define FAB$M_SHRGET	(1<<FAB$V_SHRGET)	/* 0x02 */
#define FAB$M_SHRDEL	(1<<FAB$V_SHRDEL)	/* 0x04 */
#define FAB$M_SHRUPD	(1<<FAB$V_SHRUPD)	/* 0x08 */
#define FAB$M_MSE	(1<<FAB$V_MSE)		/* 0x10 */
#define FAB$M_NIL	(1<<FAB$V_NIL)		/* 0x20 */
#define FAB$M_UPI	(1<<FAB$V_UPI)		/* 0x40 */
/* file organization (ORG) mask bits and codes */
#define FAB$V_ORG	4
#define FAB$M_ORG	0xF0
#define FAB$S_ORG	4
#define FAB$C_SEQ	0	/* sequential */
#define FAB$C_REL	16	/* relative */
#define FAB$C_IDX	32	/* indexed */
#define FAB$C_HSH	48	/* hashed */
/* record attributes (RAT) mask bits */
#define FAB$V_FTN	0
#define FAB$V_CR	1
#define FAB$V_PRN	2
#define FAB$V_BLK	3
#ifndef NO_VMS_V6
#define FAB$V_MSB	4
#endif
#define FAB$M_FTN	(1<<FAB$V_FTN)		/* 0x01 */
#define FAB$M_CR	(1<<FAB$V_CR)		/* 0x02 */
#define FAB$M_PRN	(1<<FAB$V_PRN)		/* 0x04 */
#define FAB$M_BLK	(1<<FAB$V_BLK)		/* 0x08 */
#ifndef NO_VMS_V6
#define FAB$M_MSB	(1<<FAB$V_MSB)		/* 0x10 */
#endif
/* record format (RFM) codes */
#define FAB$C_RFM_DFLT	2	/* var len is default */
#define FAB$C_UDF	0	/* undefined (also stream binary) */
#define FAB$C_FIX	1	/* fixed length records */
#define FAB$C_VAR	2	/* variable length records */
#define FAB$C_VFC	3	/* variable fixed control */
#define FAB$C_STM	4	/* RMS-11 stream (valid only for sequential org) */
#define FAB$C_STMLF	5	/* LF stream (valid only for sequential org) */
#define FAB$C_STMCR	6	/* CR stream (valid only for sequential org) */
#define FAB$C_MAXRFM	6	/* maximum rfm supported */
/* journaling (JNL) mask bits; RU, ONLY_RU, and NEVER_RU are mutually exclusive */
#define FAB$V_ONLY_RU	0
#define FAB$V_RU	1
#define FAB$V_BI	2
#define FAB$V_AI	3
#define FAB$V_AT	4
#define FAB$V_NEVER_RU	5
#define FAB$V_JOURNAL_FILE 6
#define FAB$M_ONLY_RU	(1<<FAB$V_ONLY_RU)	/* 0x01 */
#define FAB$M_RU	(1<<FAB$V_RU)		/* 0x02 */
#define FAB$M_BI	(1<<FAB$V_BI)		/* 0x04 */
#define FAB$M_AI	(1<<FAB$V_AI)		/* 0x08 */
#define FAB$M_AT	(1<<FAB$V_AT)		/* 0x10 */
#define FAB$M_NEVER_RU	(1<<FAB$V_NEVER_RU)	/* 0x20 */
#define FAB$M_JOURNAL_FILE (1<<FAB$V_JOURNAL_FILE) /* 0x40 */
/* recovery control flags (RCF) mask bits */
#define FAB$V_RCF_RU	0
#define FAB$V_RCF_AI	1
#define FAB$V_RCF_BI	2
#define FAB$M_RCF_RU	(1<<FAB$V_RCF_RU)	/* 0x01 */
#define FAB$M_RCF_AI	(1<<FAB$V_RCF_AI)	/* 0x02 */
#define FAB$M_RCF_BI	(1<<FAB$V_RCF_BI)	/* 0x04 */
/* agent access modes (ACMODES) mask bits */
#define FAB$V_LNM_MODE	0	/* logical names */
#define FAB$M_LNM_MODE	0x03
#define FAB$S_LNM_MODE	2
#define FAB$V_CHAN_MODE 2	/* channel */
#define FAB$M_CHAN_MODE 0x0C
#define FAB$S_CHAN_MODE 2
#define FAB$V_FILE_MODE 4	/* file's accessability */
#define FAB$M_FILE_MODE 0x30
#define FAB$S_FILE_MODE 2
#define FAB$V_CALLERS_MODE 6	/* structure probing */
#define FAB$M_CALLERS_MODE 0xC0
#define FAB$S_CALLERS_MODE 2

#define FAB$K_BLN	80	/* length of fab */
#define FAB$C_BLN	80

struct FAB {
    unsigned char  fab$b_bid;	/* block id */
    unsigned char  fab$b_bln;	/* block len */
    unsigned short fab$w_ifi;	/* internal file index */
    unsigned long  fab$l_fop;	/* file options */
    unsigned long  fab$l_sts;	/* status */
    unsigned long  fab$l_stv;	/* status value */
    unsigned long  fab$l_alq;	/* allocation quantity */
    unsigned short fab$w_deq;	/* default allocation quantity */
    unsigned char  fab$b_fac;	/* file access */
    unsigned char  fab$b_shr;	/* file sharing */
    unsigned long  fab$l_ctx;	/* user context */
    char	   fab$b_rtv;	/* retrieval window size */
    unsigned char  fab$b_org;	/* file organization */
    unsigned char  fab$b_rat;	/* record format */
    unsigned char  fab$b_rfm;	/* record format */
    unsigned long  fab$l_jnl;	/* journaling options */
    void	  *fab$l_xab;	/* xab address */
    void	  *fab$l_nam;	/* nam block address */
    const char	  *fab$l_fna;	/* file name string address */
    const char	  *fab$l_dna;	/* default file name string addr */
    unsigned char  fab$b_fns;	/* file name string size */
    unsigned char  fab$b_dns;	/* default name string size */
    unsigned short fab$w_mrs;	/* maximum record size */
    unsigned long  fab$l_mrn;	/* maximum record number */
    unsigned short fab$w_bls;	/* blocksize for tape */
    unsigned char  fab$b_bks;	/* bucket size */
    unsigned char  fab$b_fsz;	/* fixed header size */
    unsigned long  fab$l_dev;	/* device characteristics */
    unsigned long  fab$l_sdc;	/* spooling device characteristics */
    unsigned short fab$w_gbc;	/* global buffer count */
    unsigned char  fab$b_acmodes; /* agent access modes */
    unsigned char  fab$b_rcf;	/* (only for use by RMS Recovery) */
    unsigned	: 32;		/* long fill; (spare) */
};

struct fabdef {
    unsigned char fab$b_bid;		/* block id */
    unsigned char fab$b_bln;		/* block len */
    union {
	unsigned short fab$w_ifi;	/* internal file index */
	struct {
	    unsigned		    : 6; /* move to bit 6 */
	    unsigned fab$v_ppf_rat  : 8; /* rat value for process-permanent files */
	    unsigned fab$v_ppf_ind  : 1; /* indirect access to process-permanent file */
	    unsigned fab$v_ppifi    : 1; /* indicates that this is PPF file */
	} fab$r_ifi_bits;
    } fab$r_ifi_overlay;
    union {
	unsigned long fab$l_fop;	/* file options */
	struct {
	    unsigned fab$v_asy : 1;	/* asynchronous operations */
	    unsigned fab$v_mxv : 1;	/* maximize version number */
	    unsigned fab$v_sup : 1;	/* supersede existing file */
	    unsigned fab$v_tmp : 1;	/* create temporary file */
	    unsigned fab$v_tmd : 1;	/* create temp file marked for delete */
	    unsigned fab$v_dfw : 1;	/* deferred write (rel and idx) */
	    unsigned fab$v_sqo : 1;	/* sequential access only */
	    unsigned fab$v_rwo : 1;	/* rewind mt on open */
	    unsigned fab$v_pos : 1;	/* use next magtape position */
	    unsigned fab$v_wck : 1;	/* write checking */
	    unsigned fab$v_nef : 1;	/* inhibit end of file positioning */
	    unsigned fab$v_rwc : 1;	/* rewind mt on close */
	    unsigned fab$v_dmo : 1;	/* dismount mt on close (not implemented) */
	    unsigned fab$v_spl : 1;	/* spool file on close */
	    unsigned fab$v_scf : 1;	/* submit command file on close */
	    unsigned fab$v_dlt : 1;	/* delete sub-option */
	    unsigned fab$v_nfs : 1;	/* non-file structured operation */
	    unsigned fab$v_ufo : 1;	/* user file open - no rms operations */
	    unsigned fab$v_ppf : 1;	/* process permanent file (pio segment) */
	    unsigned fab$v_inp : 1;	/* process-permanent file is 'input' */
	    unsigned fab$v_ctg : 1;	/* contiguous extension */
	    unsigned fab$v_cbt : 1;	/* contiguous best try */
	    unsigned fab$v_syncsts : 1; /* Synchronous status notification for asynchronous routines. */
	    unsigned fab$v_rck : 1;	/* read checking */
	    unsigned fab$v_nam : 1;	/* use name block dvi, did, and/or fid fields for open */
	    unsigned fab$v_cif : 1;	/* create if non-existent */
	    unsigned fab$v_ufm : 1;	/* reserved (was UFM bitfield) */
	    unsigned fab$v_esc : 1;	/* 'escape' to non-standard function ($modify) */
	    unsigned fab$v_tef : 1;	/* truncate at eof on close (write-accessed seq. disk file only) */
	    unsigned fab$v_ofp : 1;	/* output file parse (only name type sticky) */
	    unsigned fab$v_kfo : 1;	/* known file open (image activator only release 1) */
	    unsigned	       : 1;	/* reserved (not implemented) */
	} fab$r_fop_bits;
    } fab$r_fop_overlay;
    unsigned long fab$l_sts;		/* status */
    unsigned long fab$l_stv;		/* status value */
    unsigned long fab$l_alq;		/* allocation quantity */
    unsigned short fab$w_deq;		/* default allocation quantity */
    union {
	unsigned char fab$b_fac;	/* file access */
	struct {
	    unsigned fab$v_put : 1;	/* put access */
	    unsigned fab$v_get : 1;	/* get access */
	    unsigned fab$v_del : 1;	/* delete access */
	    unsigned fab$v_upd : 1;	/* update access */
	    unsigned fab$v_trn : 1;	/* truncate access */
	    unsigned fab$v_bio : 1;	/* block i/o access */
	    unsigned fab$v_bro : 1;	/* block and record i/o access */
	    unsigned fab$v_exe : 1;	/* execute access (caller must be exec -*/
					/* or kernel mode, ufo must also be set)*/
	} fab$r_fac_bits;
    } fab$r_fac_overlay;
    union {
	unsigned char fab$b_shr;	/* file sharing */
	struct {
	    unsigned fab$v_shrput : 1;	/* put access */
	    unsigned fab$v_shrget : 1;	/* get access */
	    unsigned fab$v_shrdel : 1;	/* delete access */
	    unsigned fab$v_shrupd : 1;	/* update access */
	    unsigned fab$v_mse	  : 1;	/* multi-stream connects enabled */
	    unsigned fab$v_nil	  : 1;	/* no sharing */
	    unsigned fab$v_upi	  : 1;	/* user provided interlocking (allows */
					/* multiple writers to seq. files) */
	    unsigned		  : 1;
	} fab$r_shr_bits;
    } fab$r_shr_overlay;
    unsigned long fab$l_ctx;		/* user context */
    char fab$b_rtv;			/* retrieval window size (signed) */
    union {
	unsigned char fab$b_org;	/* file organization */
	struct {
	    unsigned		    : 4;
	    unsigned fab$v_org	    : 4;
	} fab$r_org_bits;
    } fab$r_org_overlay;
    union {
	unsigned char fab$b_rat;	/* record format */
	struct {
	    unsigned fab$v_ftn	 : 1;	/* fortran carriage-ctl */
	    unsigned fab$v_cr	 : 1;	/* lf-record-cr carriage ctl */
	    unsigned fab$v_prn	 : 1;	/* print-file carriage ctl */
	    unsigned fab$v_blk	 : 1;	/* records don't cross block boundaries */
#ifndef NO_VMS_V6
	    unsigned fab$v_msb	 : 1;	/* MSB formatted byte count */
	    unsigned		 : 3;
#else
	    unsigned		 : 4;
#endif
	} fab$r_rat_bits;
    } fab$r_rat_overlay;
    unsigned char fab$b_rfm;		/* record format */
    union {
	unsigned char fab$b_journal;	/* journaling options (from FH2$B_JOURNAL) */
	struct { /* note: only one of RU, ONLY_RU, NEVER_RU may be set at a time */
	    unsigned fab$v_only_ru : 1; /* file is accessible only in recovery unit */
	    unsigned fab$v_ru	   : 1; /* enable recovery unit journal */
	    unsigned fab$v_bi	   : 1; /* enable before image journal */
	    unsigned fab$v_ai	   : 1; /* enable after image journal */
	    unsigned fab$v_at	   : 1; /* enable audit trail journal */
	    unsigned fab$v_never_ru: 1; /* file is never accessible in recovery unit */
	    unsigned fab$v_journal_file : 1; /* this is a journal file */
	    unsigned		   : 1;
	} fab$r_journal_bits;
    } fab$r_journal_overlay;
    unsigned char fab$b_ru_facility;	/* recoverable facility id number */
    unsigned	: 16;			/* short fill; (spare) */
    unsigned long fab$l_xab;		/* xab address */
    unsigned long fab$l_nam;		/* nam block address */
    unsigned long fab$l_fna;		/* file name string address */
    unsigned long fab$l_dna;		/* default file name string addr */
    unsigned char fab$b_fns;		/* file name string size */
    unsigned char fab$b_dns;		/* default name string size */
    unsigned short fab$w_mrs;		/* maximum record size */
    unsigned long fab$l_mrn;		/* maximum record number */
    unsigned short fab$w_bls;		/* blocksize for tape */
    unsigned char fab$b_bks;		/* bucket size */
    unsigned char fab$b_fsz;		/* fixed header size */
    unsigned long fab$l_dev;		/* device characteristics */
    unsigned long fab$l_sdc;		/* spooling device characteristics */
    unsigned short fab$w_gbc;		/* global buffer count */
    union {
	unsigned char fab$b_acmodes;	/* agent access modes */
#define fab$b_dsbmsk fab$b_acmodes	/* obsolete log.trans. disable mask */
	struct {
	    unsigned fab$v_lnm_mode  : 2; /* ACMODE for log nams */
	    unsigned fab$v_chan_mode : 2; /* ACMODE for channel */
	    unsigned fab$v_file_mode : 2; /* ACMODE to use for determining file accessibility */
	    unsigned fab$v_callers_mode : 2; /* ACMODE for user structure probing; */
					     /* maximized with actual mode of caller */
	} fab$r_acmodes_bits;
    } fab$r_acmodes_overlay;
    union {				/* recovery control flags */
	unsigned char fab$b_rcf;	/* (only for use by RMS Recovery) */
	struct {
	    unsigned fab$v_rcf_ru : 1;	/* recovery unit recovery */
	    unsigned fab$v_rcf_ai : 1;	/* after image recovery */
	    unsigned fab$v_rcf_bi : 1;	/* before image recovery */
	    unsigned		  : 5;
	} fab$r_rcf_bits;
    } fab$r_rcf_overlay;
    unsigned	: 32;			/* long fill; (spare) */
};

/* declare initialized prototype data structure */
extern struct FAB cc$rms_fab __asm("_$$PsectAttributes_GLOBALSYMBOL$$cc$rms_fab");
/* globalref struct FAB cc$rms_fab; */

#endif	/*_FABDEF_H*/
