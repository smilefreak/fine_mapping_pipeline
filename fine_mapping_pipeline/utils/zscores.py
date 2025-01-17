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

import os
from os import listdir
import logging

def get_relevant_zscore(chrom, population, directory):
    """
        Scans the directory specified on the command-line by the user and checks for the specified chromosome.

    """
    zscore_files = [f for f in listdir(directory)]
    chrom = 'chr' + chrom
    z_score_file = [f for f in zscore_files if chrom in f and population in f][0]
    return os.path.join(directory, z_score_file)

def create_pos_hash_table(zscore_file):
    """
        Create position hash table
        
        TODO: Fix indel and CNV being removed from the inital analysis.

        @param pos_file the Z scores file containing the positions from impg
        
        @return pos_hash a hash table that contains the line from impG

    """
    pos_hash = {}
    with open(zscore_file) as p:
        for i, line in enumerate(p):
            line = line.strip()
            if i != 0:
                pos_hash[int(line.split()[1])] = [line.split()[4],line.split()[5]]
    return (pos_hash)

def generate_zscore_and_vcf_output(output_directory, 
                             zscore_hash, 
                             vcf,
                             locus,
                             population,
                             multiply_rsquare):
    """
        Extract vcf regions that have overlap with Impg data.

    """
    # output vcf is a temporary file that will be used downstream of this dataset.
    output_vcf = os.path.join(output_directory, locus + '.' + population + '.vcf')
    output_zscore = os.path.join(output_directory, locus + '.' + population)
    caviar_zscore = os.path.join(output_directory, locus + '.' + population + '.Z')
    with open(output_vcf, 'w') as out_vcf:
        with open(output_zscore, 'w') as out_zscore:
            with open(caviar_zscore, 'w') as out_caviar:
                for line in vcf.splitlines():
                    if "#" in line:
                        out_vcf.write(line + '\n')
                    else:
                        chrom = line.split("\t")[0]
                        pos = int(line.split('\t')[1])
                        if pos in zscore_hash.keys():
                            temp_z_score = zscore_hash[pos]
                            rsid = line.split('\t')[2]
                            out_vcf.write(line + '\n')
                            if multiply_rsquare: 
                                out_zscore.write(chrom + " " + str(pos) + " " + rsid + " " + str(float(zscore_hash[pos][0]) * float(zscore_hash[pos][1])) +'\n')
                            else:
                                out_zscore.write(chrom + " " + str(pos) + " " + rsid + " " + zscore_hash[pos][0] +'\n')
                            out_caviar.write(rsid+ ' ' + zscore_hash[pos][0] +'\n')
    return output_vcf
