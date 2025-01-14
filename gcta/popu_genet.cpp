/*
 * GCTA: a tool for Genome-wide Complex Trait Analysis
 *
 * Calculating population characteristics using SNP data
 *
 * 2013 by Jian Yang <jian.yang@uq.edu.au>
 *
 * This file is distributed under the GNU General Public
 * License, Version 2.  Please see the file COPYING for more
 * details
 */

#include "gcta.h"

// paa: proportion of ancestral alleles
void gcta::paa(string aa_file)
{
    check_autosome();
    if(_mu.empty()) calcu_mu();
	//rm_high_ld();
    
    int i=0, j=0, k=0;
    
    // read ancestral alleles from a file
    ifstream i_aa(aa_file.c_str());
    if(!i_aa) throw("Error: can not open the file ["+aa_file+"] to read.");
    string cbuf=".";
    string str_buf;
	cout<<"Reading ancestral alleles of the SNPs from ["+aa_file+"]."<<endl;
    map<string, int>::iterator iter, End=_snp_name_map.end();
    vector<string> aa(_snp_num);
    for(i=0; i<_snp_num; i++) aa[i]=".";
    int icount=0;
	while(i_aa){
		i_aa>>str_buf;
		if(i_aa.eof()) break;
		iter=_snp_name_map.find(str_buf);
		i_aa>>cbuf;
		if(iter!=End && cbuf!="."){
		    aa[iter->second]=cbuf;
		    icount++;
		}
	}
    i_aa.close();
	cout<<"Ancestral alleles for "<<icount<<" SNPs are included from ["+aa_file+"]."<<endl;
    
	cout<<"Calculating proportion of ancestral alleles ..."<<endl;
	double x=0.0;
	vector<double> hom_aa_rare(_keep.size()), hom_aa_comm(_keep.size()), hom_da_rare(_keep.size()), hom_da_comm(_keep.size()), het_aa_rare(_keep.size()), het_aa_comm(_keep.size()), nomiss(_keep.size());
	for(i=0; i<_keep.size(); i++){
 		for(k=0; k<_include.size(); k++){
 		    if(aa[_include[k]]==".") continue;
            if(!_snp_1[_include[k]][_keep[i]] || _snp_2[_include[k]][_keep[i]]){
                x=_snp_1[_include[k]][_keep[i]]+_snp_2[_include[k]][_keep[i]];
                if(x<0.1){
                    if(_ref_A[_include[k]]==aa[_include[k]]){
                        if(_mu[_include[k]]>1.0) hom_da_rare[i]+=1.0;
                        else hom_da_comm[i]+=1.0;
                    }
                    else{
                        if(_mu[_include[k]]>1.0) hom_aa_rare[i]+=1.0;
                        else hom_aa_comm[i]+=1.0;
                    }
                }
                else if(x>1.9){
                    if(_ref_A[_include[k]]==aa[_include[k]]){
                        if(_mu[_include[k]]>1.0) hom_aa_comm[i]+=1.0;
                        else hom_aa_rare[i]+=1.0;
                    }
                    else{
                        if(_mu[_include[k]]>1.0) hom_da_comm[i]+=1.0;
                        else hom_da_rare[i]+=1.0;
                    }
                }
                else{
                    if(_ref_A[_include[k]]==aa[_include[k]]){
                        if(_mu[_include[k]]>1.0) het_aa_comm[i]+=1.0;
                        else het_aa_rare[i]+=1.0;
                    }
                    else{
                        if(_mu[_include[k]]>1.0) het_aa_rare[i]+=1.0;
                        else het_aa_comm[i]+=1.0;
                    }
                }
                nomiss[i]+=1.0;
            }
        }
        hom_aa_rare[i]/=nomiss[i];
        hom_aa_comm[i]/=nomiss[i];
        hom_da_rare[i]/=nomiss[i];
        hom_da_comm[i]/=nomiss[i];
        het_aa_rare[i]/=nomiss[i];
        het_aa_comm[i]/=nomiss[i];
        cout<<i+1<<" of "<<_keep.size()<<" individuals.\r";
	}
    
    // Save matrix A in binary file
	string paa_file=_out+".paa";
	ofstream o_paa(paa_file.c_str());
	if(!o_paa) throw("Error: can not open the file ["+paa_file+"] to write.");
	o_paa<<"FID\tIID\tNOMISS\tHOM_AA_RARE\tHOM_AA_COMM\tHOM_DA_RARE\tHOM_DA_COMM\tHET_AA_RARE\tHET_AA_COMM"<<endl;
	for(i=0; i<_keep.size(); i++) o_paa<<_fid[i]<<"\t"<<_pid[i]<<"\t"<<nomiss[i]<<"\t"<<hom_aa_rare[i]<<"\t"<<hom_aa_comm[i]<<"\t"<<hom_da_rare[i]<<"\t"<<hom_da_comm[i]<<"\t"<<het_aa_rare[i]<<"\t"<<het_aa_comm[i]<<endl;
	o_paa.close();
	cout<<"Proportion of ancestral alleles has been saved in file ["+paa_file+"]."<<endl;
}

// inbreeding coefficient
void gcta::ibc(bool ibc_all)
{
    check_autosome();
    if(_mu.empty()) calcu_mu();
	//rm_high_ld();
    
    int i=0, j=0, k=0;
    
	// Calcuate A matrix
	cout<<"Calculating the inbreeding coefficients ..."<<endl;
	vector<double> h(_include.size()), h_i(_include.size()), w(_include.size()), p(_include.size()), p_q(_include.size()), q_p(_include.size()); // variance of each SNP, 2pq
	#pragma omp parallel for
    for(j=0; j<_include.size(); j++){
	    p[j]=0.5*_mu[_include[j]];
        h[j]=2.0*p[j]*(1.0-p[j]);
        p_q[j]=p[j]/(1.0-p[j]);
        if(fabs(p_q[j])<1.0e-50) q_p[j]=0.0;
        else q_p[j]=1.0/p_q[j];
        w[j]=h[j]/(1.0-h[j]);
        if(fabs(h[j])<1.0e-50) h_i[j]=0.0;
        else h_i[j]=1.0/h[j];
	}
	vector<double> Fhat1, Fhat1_w, Fhat2, Fhat2_w, Fhat3, Fhat4, Fhat5, Fhat6, Fhat7, rare_hom, comm_hom, nomiss;
    Fhat1.resize(_keep.size());
    Fhat2.resize(_keep.size());
    Fhat3.resize(_keep.size());
    nomiss.resize(_keep.size());
    if(ibc_all){
        Fhat4.resize(_keep.size());
        Fhat5.resize(_keep.size());
        Fhat6.resize(_keep.size());
        Fhat7.resize(_keep.size());
        Fhat1_w.resize(_keep.size());
        Fhat2_w.resize(_keep.size());
        rare_hom.resize(_keep.size());
        comm_hom.resize(_keep.size());
    }
    #pragma omp parallel for private(k)
    for(i=0; i<_keep.size(); i++){
        double x=0.0, sum_w=0.0, sum_h=0.0, Fhat_buf=0.0;
		for(k=0; k<_include.size(); k++){
            if(!_snp_1[_include[k]][_keep[i]] || _snp_2[_include[k]][_keep[i]]){
                x=_snp_1[_include[k]][_keep[i]]+_snp_2[_include[k]][_keep[i]];
                if(_allele2[_include[k]]==_ref_A[_include[k]]) x=2.0-x;
                Fhat_buf=(x-_mu[_include[k]])*(x-_mu[_include[k]]);
                if(ibc_all) Fhat4[i]+=Fhat_buf;
                Fhat_buf*=h_i[k];
                Fhat1[i]+=Fhat_buf;
                if(ibc_all) Fhat1_w[i]+=Fhat_buf*w[k];
                Fhat_buf=h[k]-x*(2.0-x);
                if(ibc_all) Fhat6[i]+=Fhat_buf;
                Fhat_buf*=h_i[k];
                Fhat2[i]+=Fhat_buf;
                if(ibc_all) Fhat2_w[i]+=Fhat_buf*w[k];
                Fhat_buf=(x*(x-1.0-_mu[_include[k]])+_mu[_include[k]]*p[k]);
                Fhat3[i]+=Fhat_buf*h_i[k];
                if(ibc_all){
                    Fhat5[i]+=Fhat_buf;
                    sum_w+=w[k];
                    sum_h+=h[k];
                    if(x<0.1) Fhat7[i]+=p_q[k];
                    else if(x>1.9) Fhat7[i]+=q_p[k];
                    // Count the number of rare and common homozygotes
                    if(x<0.1){
                        if(p[k]>0.5) rare_hom[i]+=1.0;
                        else comm_hom[i]+=1.0;
                    }
                    else if(x>1.9){
                        if(p[k]<0.5) rare_hom[i]+=1.0;
                        else comm_hom[i]+=1.0;
                    }
                }
                nomiss[i]+=1.0;
            }
        }
        Fhat1[i]/=nomiss[i];
        Fhat2[i]/=nomiss[i];
        Fhat3[i]/=nomiss[i];
        if(ibc_all){
            Fhat1_w[i]/=sum_w;
            Fhat2_w[i]/=sum_w;
            Fhat4[i]/=sum_h;
            Fhat5[i]/=sum_h;
            Fhat6[i]/=sum_h;
            rare_hom[i]/=nomiss[i];
            comm_hom[i]/=nomiss[i];
            Fhat7[i]/=nomiss[i];
        }
	}
    
    // Save matrix A in binary file
	string ibc_file=_out+".ibc";
	ofstream o_ibc(ibc_file.c_str());
	if(!o_ibc) throw("Error: can not open the file ["+ibc_file+"] to write.");
	if(ibc_all){
        o_ibc<<"FID\tIID\tNOMISS\tP_RARE_HOM\tP_COMM_HOM\tFhat1\tFhat1_w\tFhat2\tFhat2_w\tFhat3\tFhat4\tFhat5\tFhat6\tFhat7"<<endl;
        for(i=0; i<_keep.size(); i++) o_ibc<<_fid[_keep[i]]<<"\t"<<_pid[_keep[i]]<<"\t"<<nomiss[i]<<"\t"<<rare_hom[i]<<"\t"<<comm_hom[i]<<"\t"<<Fhat1[i]-1.0<<"\t"<<Fhat1_w[i]-1.0<<"\t"<<Fhat2[i]<<"\t"<<Fhat2_w[i]<<"\t"<<Fhat3[i]<<"\t"<<Fhat4[i]-1.0<<"\t"<<Fhat5[i]<<"\t"<<Fhat6[i]<<"\t"<<Fhat7[i]<<endl;
	}
	else{
        o_ibc<<"FID\tIID\tNOMISS\tFhat1\tFhat2\tFhat3"<<endl;
        for(i=0; i<_keep.size(); i++) o_ibc<<_fid[_keep[i]]<<"\t"<<_pid[_keep[i]]<<"\t"<<nomiss[i]<<"\t"<<Fhat1[i]-1.0<<"\t"<<Fhat2[i]<<"\t"<<Fhat3[i]<<endl;
	}
	o_ibc.close();
	cout<<"Inbreeding coefficients have been saved in the file ["+ibc_file+"]."<<endl;
}