#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "genotq.h"

int convert_help();
int bcf_wahbm(char *in,
              char *wah_out,
              char *bim_out,
              char *vid_out,
              char *tmp_dir,
              uint32_t num_fields,
              uint32_t num_records);
int ped_ped(char *in, char *ped, uint32_t col, char *out);

int convert(int argc, char **argv)
{
    if (argc < 2) return convert_help();

    int c;
    char *in=NULL, *out=NULL, *bim=NULL, *vid=NULL, *tmp_dir=NULL, *ped=NULL;
    uint32_t num_fields, num_records, col = 2;
    int i_is_set = 0, 
        o_is_set = 0, 
        f_is_set = 0, 
        b_is_set = 0, 
        v_is_set = 0, 
        t_is_set = 0, 
        p_is_set = 0, 
        r_is_set = 0; 

    while((c = getopt (argc, argv, "hi:o:f:r:b:v:t:p:c:")) != -1) {
        switch (c) {
            case 'c':
                col = atoi(optarg);
                break;
            case 'p':
                p_is_set = 1;
                ped = optarg;
                break;
            case 't':
                t_is_set = 1;
                tmp_dir = optarg;
                break;
            case 'v':
                v_is_set = 1;
                vid = optarg;
                break;
            case 'b':
                b_is_set = 1;
                bim = optarg;
                break;
            case 'i':
                i_is_set = 1;
                in = optarg;
                break;
            case 'o':
                o_is_set = 1;
                out = optarg;
                break;
            case 'f':
                f_is_set = 1;
                num_fields = atoi(optarg);
                break;
            case 'r':
                r_is_set = 1;
                num_records = atoi(optarg);
                break;
            case 'h':
                convert_help();
                return 1;
            case '?':
                if ( (optopt == 'i') || 
                     (optopt == 'f') ||
                     (optopt == 'r') ||
                     (optopt == 't') ||
                     (optopt == 's') ||
                     (optopt == 'p') ||
                     (optopt == 'c') ||
                     (optopt == 'o') )
                    fprintf (stderr, "Option -%c requires an argument.\n",
                            optopt);
                else if (isprint (optopt))
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                else
                fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
            default:
                convert_help();
                return 1;
        }
    }

    char *type = argv[0];

    if (i_is_set == 0) {
        fprintf(stderr, "BCF/VCF/VCF.GZ file is not set\n");
        return convert_help();
    } else {
        if ( access( in, F_OK) == -1 )
            err(EX_NOINPUT,
                "Error accessing BCF/VCF/VCF.GZ file \"%s\"",
                in);
    }

    if (strcmp(type, "bcf") == 0) {
        if ( (f_is_set == 0) || (r_is_set == 0) ) {

            fprintf(stderr,"Attempting to autodetect number of variants "
                    "and samples from %s\n", in);
            //Try and auto detect the sizes, need the index
            tbx_t *tbx = NULL;
            hts_idx_t *idx = NULL;
            htsFile *fp    = hts_open(in,"rb");
            if ( !fp ) {
                err(EX_DATAERR, "Could not read file: %s", in);
            }

            bcf_hdr_t *hdr = bcf_hdr_read(fp);
            if ( !hdr ) {
                err(EX_DATAERR, "Could not read the header: %s", in);
            }

            if (hts_get_format(fp)->format==vcf) {
                tbx = tbx_index_load(in);
                if ( !tbx ) { 
                    err(EX_NOINPUT,"Could not load TBI index: %s.tbi", in);
                }
            } else if ( hts_get_format(fp)->format==bcf ) {
                idx = bcf_index_load(in);
                if ( !idx ) {
                    err(EX_NOINPUT,"Could not load CSI index: %s.csi", in);
                }
            } else {
                err(EX_NOINPUT,
                    "Could not detect the file type as VCF or BCF: %s",
                    in);
            }

            num_fields = hdr->n[BCF_DT_SAMPLE];

            num_records = 0;
            const char **seq;
            int nseq;
            seq = tbx ? tbx_seqnames(tbx, &nseq) : 
                    bcf_index_seqnames(idx, hdr, &nseq);
            int i;
            uint32_t sum = 0;
            for (i = 0; i < nseq; ++i) {
                uint64_t records, v;
                hts_idx_get_stat(tbx ? tbx->idx: idx, i, &records, &v);
                num_records += records;
            }

            fprintf(stderr, "Number of variants:%u\tNumber of samples:%u\n",
                    num_records, num_fields);
            free(seq);
            hts_close(fp);
            bcf_hdr_destroy(hdr);
            if (idx)
                hts_idx_destroy(idx);
            if (tbx)
                tbx_destroy(tbx);
        }


        if (o_is_set == 0) {
            out  = (char*)malloc(strlen(in) + 5); // 5 for ext and \0
            if (!out)
                err(EX_OSERR, "malloc error");
            strcpy(out,in);
            strcat(out, ".gqt");
        }
        if (b_is_set == 0) {
            bim  = (char*)malloc(strlen(in) + 5); // 5 for ext and \0
            if (!bim)
                err(EX_OSERR, "malloc error");
            strcpy(bim,in);
            strcat(bim, ".bim");
        }
        if (v_is_set == 0) {
            vid  = (char*)malloc(strlen(in) + 5); // 5 for ext and \0
            if (!vid)
                err(EX_OSERR, "malloc error");
            strcpy(vid,in);
            strcat(vid, ".vid");
        }
        if (t_is_set == 0) {
            tmp_dir  = (char*)malloc(3*sizeof(char)); // "./\0"
            if (!tmp_dir  )
                err(EX_OSERR, "malloc error");
            strcpy(tmp_dir,"./");
        }

        int r = bcf_wahbm(in, out, bim, vid, tmp_dir, num_fields, num_records);

        return r;
    } 

    if (strcmp(type, "ped") == 0)  {
        if (o_is_set == 0) {
            if (p_is_set == 1) {
                out  = (char*)malloc(strlen(ped) + 4); // 4 for ext and \0
                if (!out)
                    err(EX_OSERR, "malloc error");
                strcpy(out,ped);
                strcat(out, ".db");
            } else {
                out  = (char*)malloc(strlen(in) + 4); // 4 for ext and \0
                if (!out)
                    err(EX_OSERR, "malloc error");
                strcpy(out,in);
                strcat(out, ".db");
            }
      }

      return ped_ped(in, ped, col, out);
    }
    return convert_help();
}

int convert_help()
{
    fprintf(stderr,
            "%s v%s\n"
            "usage:   gqt convert <type> -i <input VCF/VCF.GZ/BCF file>\n"
            "     types:\n"
            "         bcf         create a GQT index\n"
            "         ped         create sample phenotype database\n\n"
            "     options:\n"
            "         -p           PED file name (opt.)\n"
            "         -c           Sample name column in PED (Default 2)\n"
            "         -o           Output file name (opt.)\n"
            "         -v           VID output file name (opt.)\n"
            "         -b           BIM output file name (opt.)\n"
            "         -r           Number of variants (opt. with index)\n"
            "         -f           Number of samples (opt. with index)\n"
            "         -t           Tmp working directory(./ by defualt)\n",
            PROGRAM_NAME, VERSION);

    return EX_USAGE;
}


int ped_ped(char *in, char *ped, uint32_t col, char *out)
{
    return convert_file_by_name_ped_to_db(in, ped, col, out);
}

int bcf_wahbm(char *in,
              char *wah_out,
              char *bim_out,
              char *vid_out,
              char *tmp_dir,
              uint32_t num_fields,
              uint32_t num_records)
{
    return convert_file_by_name_bcf_to_wahbm_bim(in,
                                                 num_fields,
                                                 num_records,
                                                 wah_out,
                                                 bim_out,
                                                 vid_out,
                                                 tmp_dir);
}
