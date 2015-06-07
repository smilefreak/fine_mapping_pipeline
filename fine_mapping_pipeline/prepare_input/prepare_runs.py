# Copyright (c) 2015 Boocock James <james.boocock@otago.ac.nz>
# Author: Boocock James <james.boocock@otago.ac.nz>
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
import logging

import fine_mapping_pipeline.ucsc
## Import datetime to make dated directory
import time, datetime, os

from fine_mapping_pipeline.expections.error_codes import *

from fine_mapping_pipeline.plink.ld import vcf_to_plink, plink_to_ld_matrix
from fine_mapping_pipeline.snp_list import SnpList
from fine_mapping_pipeline.onekg_utilities.obtain_vcf import get_vcf_file
from fine_mapping_pipeline.onekg_utilities.vcf_filter import extract_population_from_1000_genomes
from fine_mapping_pipeline.gemini.create import create_gemini_database
from fine_mapping_pipeline.gemini.annotation import generate_and_write_encode_annotations
from fine_mapping_pipeline.utils.zscores import get_relevant_zscore, create_pos_hash_table, generate_zscore_and_vcf_output
from fine_mapping_pipeline.finemap.paintor import run_paintor
from fine_mapping_pipeline.finemap.caviarbf import run_caviarbf

def _prepare_output_dir(output_directory):
    if output_directory is None:
        todaystr = 'fine_mapping_run' + datetime.date.today().isoformat()
        output_directory = todaystr
        try:
            os.mkdir(todaystr)
        except OSError:
            pass
    else:
        output_directory = output_directory
    if not output_directory.endswith('/'):
        output_directory += '/'
    return output_directory

def prepare_runs(args):
    """
        Parses arguments from the paintor sub-command and processes data for use in Paintor.

    """
    output_directory = _prepare_output_dir(args.output_directory)
    z_score_dir = args.z_score_dir
    try:
        flanking_region = int(args.flanking_region)
    except ValueError:
        logging.error('Flanking region argument needs to be an integer')
        sys.exit(COMMAND_LINE_ERROR)
    build = args.build
    # Create the SNPList
    snp_list = SnpList(args.snp_list, build)
    logging.info(snp_list)
    # Locus to process
    # population_to_extract_vcf
    no_flanking = args.flanking_units
    if no_flanking:
        raise NotImplementedError("Using a number of flanking SNPs instead of a region is not supported")
    population = args.population
    loci = []
    gemini_databases = []
    for snp in snp_list:
        logging.info('Preparing output files for SNP {0}'.format(snp.rsid))
        locus = snp.rsid
        loci.append(locus)
        logging.info("Obtaining VCF file from the 1000 genomes project")
        vcf = get_vcf_file(snp, flanking_region)
        vcf = extract_population_from_1000_genomes(vcf=vcf, super_population=population)
        z_score_file = get_relevant_zscore(snp.chrom, z_score_dir)
        pos_list_zscore = create_pos_hash_table(z_score_file)
        output_vcf = generate_zscore_and_vcf_output(output_directory=output_directory, zscore_hash=pos_list_zscore, vcf=vcf, locus=locus)
        logging.info("Creating gemini database")
        gemini_databases.append(create_gemini_database(vcf=output_vcf))
        logging.info("Creating LD matrix using plink")
        vcf_to_plink(locus, output_directory=output_directory,
                     vcf=output_vcf)
        plink_to_ld_matrix(locus, output_directory=output_directory)
    logging.info("Generating annotation matrices to be used with Paintor")
    logging.info(gemini_databases)
    generate_and_write_encode_annotations(databases=gemini_databases, output_directory=output_directory, loci=snp_list)
    with open(os.path.join(output_directory, 'input.files'), 'w') as out_f:
        for snp in snp_list:
            out_f.write(snp.rsid +'\n')