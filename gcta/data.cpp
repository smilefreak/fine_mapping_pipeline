/*
 * GCTA: a tool for Genome-wide Complex Trait Analysis
 *
 * Implementations of functions for data management
 *
 * 2010 by Jian Yang <jian.yang@uq.edu.au>
 *
 * This file is distributed under the GNU General Public
 * License, Version 2.  Please see the file COPYING for more
 * details
 */

#include "gcta.h"

gcta::gcta(int autosome_num, string out)
{
    _autosome_num=autosome_num;
    _out=out;
    _dosage_flag=false;
	_grm_bin_flag=false;
    _reml_mtd=0;
    _reml_inv_mtd=0;
    _reml_max_iter=30;
    _jma_actual_geno=false;
    _jma_wind_size=1e7;
	_jma_p_cutoff=5e-8;
	_jma_collinear=0.9;
	_jma_Vp=1;
	_GC_val=1;
    _bivar_reml=false;
    _ignore_Ce=false;
    _bivar_no_constrain=false;
    _y_Ssq=0.0;
    _y2_Ssq=0.0;
    _ncase=0;
    _ncase2=0;
    _flag_CC=false;
    _flag_CC2=false;
    _reml_have_bend_A=false;
    _V_inv_mtd=0;
}

gcta::gcta()
{
    _dosage_flag=false;
	_grm_bin_flag=false;
    _reml_mtd=0;
    _reml_inv_mtd=0;
    _reml_max_iter=30;
    _jma_actual_geno=false;
    _jma_wind_size=1e7;
	_jma_p_cutoff=5e-8;
	_jma_collinear=0.9;
	_jma_Vp=1;
	_GC_val=1;
    _bivar_reml=false;
    _ignore_Ce=false;
    _bivar_no_constrain=false;
    _y_Ssq=0.0;
    _y2_Ssq=0.0;
    _ncase=0;
    _ncase2=0;
    _flag_CC=false;
    _flag_CC2=false;
    _reml_have_bend_A=false;
    _V_inv_mtd=0;
}

gcta::~gcta()
{

}

void gcta::read_famfile(string famfile)
{
	ifstream Fam(famfile.c_str());
	if(!Fam) throw("Error: can not open the file ["+famfile+"] to read.");
	cout<<"Reading PLINK FAM file from ["+famfile+"]."<<endl;

	int i=0;
	string str_buf;
	_fid.clear();
	_pid.clear();
	_fa_id.clear();
	_mo_id.clear();
	_sex.clear();
	_pheno.clear();
	while(Fam){
	    Fam>>str_buf;
	    if(Fam.eof()) break;
		_fid.push_back(str_buf);
	    Fam>>str_buf;
		_pid.push_back(str_buf);
	    Fam>>str_buf;
		_fa_id.push_back(str_buf);
	    Fam>>str_buf;
		_mo_id.push_back(str_buf);
		Fam>>str_buf;
		_sex.push_back(atoi(str_buf.c_str()));
		Fam>>str_buf;
		_pheno.push_back(atoi(str_buf.c_str()));
	}
	Fam.clear();
	Fam.close();
	_indi_num=_fid.size();
	cout<<_indi_num<<" individuals to be included from ["+famfile+"]."<<endl;

	// Initialize _keep
	init_keep();
}

void gcta::init_keep()
{
	_keep.clear();
	_keep.resize(_indi_num);
	_id_map.clear();
    int i=0, size=0;
	for(i=0; i<_indi_num; i++){
	    _keep[i]=i;
        _id_map.insert(pair<string, int>(_fid[i]+":"+_pid[i], i));
        if(size==_id_map.size()) throw("Error: Duplicate individual ID found: \""+_fid[i]+"\t"+_pid[i]+"\".");
        size=_id_map.size();
	}
}

void gcta::read_bimfile(string bimfile)
{
	// Read bim file: recombination rate is defined between SNP i and SNP i-1
	int ibuf=0;
	string cbuf="0";
	double dbuf=0.0;
	string str_buf;
	ifstream Bim(bimfile.c_str());
	if(!Bim) throw("Error: can not open the file ["+bimfile+"] to read.");
	cout<<"Reading PLINK BIM file from ["+bimfile+"]."<<endl;
	_chr.clear();
	_snp_name.clear();
	_genet_dst.clear();
	_bp.clear();
	_allele1.clear();
	_allele2.clear();
	while(Bim){
	    Bim>>ibuf;
	    if(Bim.eof()) break;
		_chr.push_back(ibuf);
		Bim>>str_buf;
		_snp_name.push_back(str_buf);
		Bim>>dbuf;
		_genet_dst.push_back(dbuf);
		Bim>>ibuf;
		_bp.push_back(ibuf);
		Bim>>cbuf;
        //StrFunc::to_upper(cbuf);
		_allele1.push_back(cbuf);
		Bim>>cbuf;
        //StrFunc::to_upper(cbuf);
		_allele2.push_back(cbuf);
	}
	Bim.close();
	_snp_num=_chr.size();
	_ref_A=_allele1;
    _other_A=_allele2;
	cout<<_snp_num<<" SNPs to be included from ["+bimfile+"]."<<endl;

	// Initialize _include
	init_include();
}

void gcta::init_include()
{
	_include.clear();
	_include.resize(_snp_num);
	_snp_name_map.clear();
    int i=0, size=0;
	for(i=0; i<_snp_num; i++){
	    _include[i]=i;
	    _snp_name_map.insert(pair<string, int>(_snp_name[i], i));
        if(size==_snp_name_map.size()) throw("Error: Duplicate SNP name found: \""+_snp_name[i]+"\".");
        size=_snp_name_map.size();
	}
}

// some code are adopted from PLINK with modifications
void gcta::read_bedfile(string bedfile)
{
	int i=0, j=0, k=0;

	// Flag for reading individuals and SNPs
	vector<int> rindi, rsnp;
	get_rindi(rindi);
	get_rsnp(rsnp);

	if(_include.size()==0) throw("Error: No SNP is retained for analysis.");
	if(_keep.size()==0) throw("Error: No individual is retained for analysis.");

	// Read bed file
    char ch[1];
    bitset<8> b;
    _snp_1.resize(_include.size());
    _snp_2.resize(_include.size());
    for(i=0; i<_include.size(); i++){
        _snp_1[i].reserve(_keep.size());
        _snp_2[i].reserve(_keep.size());
    }
	fstream BIT(bedfile.c_str(), ios::in|ios::binary);
	if(!BIT) throw("Error: can not open the file ["+bedfile+"] to read.");
	cout<<"Reading PLINK BED file from ["+bedfile+"] in SNP-major format ..."<<endl;
    for(i=0; i<3; i++) BIT.read(ch,1); // skip the first three bytes
    int snp_indx=0, indi_indx=0;
    for(j=0, snp_indx=0; j<_snp_num; j++){ // Read genotype in SNP-major mode, 00: homozygote AA; 11: homozygote BB; 01: hetezygote; 10: missing
        if(!rsnp[j]){
            for(i=0; i<_indi_num; i+=4) BIT.read(ch,1);
            continue;
        }
        for(i=0, indi_indx=0; i<_indi_num;){
			BIT.read(ch,1);
            if(!BIT) throw("Error: problem with the BED file ... has the FAM/BIM file been changed?");
            b=ch[0];
            k=0;
            while(k < 7 && i < _indi_num){ // change code: 11 for AA; 00 for BB;
                if(!rindi[i]) k+=2;
                else{
                    _snp_2[snp_indx][indi_indx]=(!b[k++]);
                    _snp_1[snp_indx][indi_indx]=(!b[k++]);
                    indi_indx++;
                }
                i++;
           }
		}
		if(snp_indx==_include.size()) break;
		snp_indx++;
    }
	BIT.clear();
    BIT.close();
	cout<<"Genotype data for "<<_keep.size()<<" individuals and "<<_include.size()<<" SNPs to be included from ["+bedfile+"]."<<endl;

    update_fam(rindi);
	update_bim(rsnp);
}

void gcta::get_rsnp(vector<int> &rsnp)
{
    rsnp.clear();
    rsnp.resize(_snp_num);
	for(int i=0; i<_snp_num; i++){
	    if(_snp_name_map.find(_snp_name[i])!=_snp_name_map.end()) rsnp[i]=1;
	    else rsnp[i]=0;
	}
}

void gcta::get_rindi(vector<int> &rindi)
{
	rindi.clear();
	rindi.resize(_indi_num);
	for(int i=0; i<_indi_num; i++){
	    if(_id_map.find(_fid[i]+":"+_pid[i])!=_id_map.end()) rindi[i]=1;
	    else rindi[i]=0;
	}
}

void gcta::update_bim(vector<int> &rsnp)
{
    int i=0;

	//update bim information
	vector<int> chr_buf, bp_buf;
	vector<string> a1_buf, a2_buf, ref_A_buf, other_A_buf;
	vector<string> snp_name_buf;
	vector<double> genet_dst_buf;
	for(i=0; i<_snp_num; i++){
	    if(!rsnp[i]) continue;
	    chr_buf.push_back(_chr[i]);
	    snp_name_buf.push_back(_snp_name[i]);
	    genet_dst_buf.push_back(_genet_dst[i]);
        bp_buf.push_back(_bp[i]);
	    a1_buf.push_back(_allele1[i]);
	    a2_buf.push_back(_allele2[i]);
	    ref_A_buf.push_back(_ref_A[i]);
        other_A_buf.push_back(_other_A[i]);
	}
	_chr.clear(); _snp_name.clear(); _genet_dst.clear(); _bp.clear(); _allele1.clear(); _allele2.clear(); _ref_A.clear(); _other_A.clear();
	_chr=chr_buf; _snp_name=snp_name_buf; _genet_dst=genet_dst_buf; _bp=bp_buf; _allele1=a1_buf; _allele2=a2_buf; _ref_A=ref_A_buf; _other_A=other_A_buf;
	_snp_num=_chr.size();
	_include.clear();
	_include.resize(_snp_num);
	_snp_name_map.clear();
    
	for(i=0; i<_snp_num; i++){
	    _include[i]=i;
	    _snp_name_map.insert(pair<string, int>(_snp_name[i], i));
	}
}

void gcta::update_fam(vector<int> &rindi)
{
	//update fam information
    int i=0;
    vector<string> fid_buf, pid_buf, fa_id_buf, mo_id_buf;
	vector<int> sex_buf;
	vector<double> pheno_buf;
	for(i=0; i<_indi_num; i++){
	    if(!rindi[i]) continue;
	    fid_buf.push_back(_fid[i]);
        pid_buf.push_back(_pid[i]);
        fa_id_buf.push_back(_fa_id[i]);
        mo_id_buf.push_back(_mo_id[i]);
        sex_buf.push_back(_sex[i]);
        pheno_buf.push_back(_pheno[i]);
	}
	_fid.clear(); _pid.clear(); _fa_id.clear(); _mo_id.clear(); _sex.clear(); _pheno.clear(); 
    _fid=fid_buf; _pid=pid_buf; _fa_id=fa_id_buf; _mo_id=mo_id_buf; _sex=sex_buf; _pheno=pheno_buf;
    
	_indi_num=_fid.size();
	_keep.clear();
	_keep.resize(_indi_num);
	_id_map.clear();
	for(i=0; i<_indi_num; i++){
	    _keep[i]=i;
        _id_map.insert(pair<string, int>(_fid[i]+":"+_pid[i], i));
	}
}

void gcta::read_imp_info_mach(string zinfofile)
{
    _dosage_flag=true;

    int i=0;
    const int MAX_LINE_LENGTH = 1000;
    char buf[MAX_LINE_LENGTH];
    gzifstream zinf;
    zinf.open( zinfofile.c_str() );
    if(! zinf.is_open()) throw("Error: can not open the file ["+zinfofile+"] to read.");

    string str_buf, errmsg="Reading dosage data failed. Please check the format of the map file.";
    string c_buf;
    double f_buf=0.0;
    cout<<"Reading map file of the imputed dosage data from ["+zinfofile+"]."<<endl;
    zinf.getline(buf, MAX_LINE_LENGTH, '\n'); // skip the header
    vector<string> vs_buf;
    int col_num=StrFunc::split_string(buf, vs_buf);
    if(col_num<7) throw(errmsg);
    if(vs_buf[6]!="Rsq") throw(errmsg);
    _snp_name.clear();
    _allele1.clear();
    _allele2.clear();
    _impRsq.clear();
    while(1){
        zinf.getline(buf, MAX_LINE_LENGTH, '\n');
        stringstream ss(buf);
        string nerr=errmsg+"\nError occurs in line: "+ss.str();
        if(!(ss>>str_buf)) break;
        _snp_name.push_back(str_buf);
        if(!(ss>>c_buf)) throw(nerr);
        _allele1.push_back(c_buf);
        if(!(ss>>c_buf)) throw(nerr);
        _allele2.push_back(c_buf);
        for(i=0; i<4; i++) if(!(ss>>f_buf)) throw(nerr);
        _impRsq.push_back(f_buf);
        if(zinf.fail() || !zinf.good()) break;
    }
    zinf.clear();
    zinf.close();
    _snp_num=_snp_name.size();
    _chr.resize(_snp_num);
    _bp.resize(_snp_num);
    _genet_dst.resize(_snp_num);
    _ref_A=_allele1;
    _other_A=_allele2;

	// Initialize _include
	init_include();
    cout<<_snp_num<<" SNPs to be included from ["+zinfofile+"]."<<endl;
}

void gcta::read_imp_dose_mach(string zdosefile, string kp_indi_file, string rm_indi_file, string blup_indi_file)
{
	if(_include.size()==0) throw("Error: No SNP is retained for analysis.");
    
    int i=0, j=0, k=0, line=0;
    vector<int> rsnp;
    get_rsnp(rsnp);
    
    const int MAX_LINE_LENGTH = 10000000;
    char buf[MAX_LINE_LENGTH];
    gzifstream zinf;
    zinf.open(zdosefile.c_str());
    if(! zinf.is_open()) throw("Error: can not open the file ["+zdosefile+"] to read.");
    
    vector<string> indi_ls;
    map<string, int> kp_id_map, blup_id_map, rm_id_map;
    bool kp_indi_flag=!kp_indi_file.empty(), blup_indi_flag=!blup_indi_file.empty(), rm_indi_flag=!rm_indi_file.empty();
    if(kp_indi_flag) read_indi_list(kp_indi_file, indi_ls);
    for(i=0; i<indi_ls.size(); i++) kp_id_map.insert(pair<string, int>(indi_ls[i], i));
    if(blup_indi_flag) read_indi_list(blup_indi_file, indi_ls);
    for(i=0; i<indi_ls.size(); i++) blup_id_map.insert(pair<string, int>(indi_ls[i], i));
    if(rm_indi_flag) read_indi_list(rm_indi_file, indi_ls);
    for(i=0; i<indi_ls.size(); i++) rm_id_map.insert(pair<string, int>(indi_ls[i], i));
    
	bool missing=false;
    string str_buf, id_buf, err_msg="Error: reading dosage data failed. Are the map file and the dosage file matched?";
    double f_buf=0.0;
    vector<string> kept_id, vs_buf;
    cout<<"Reading dosage data from ["+zdosefile+"] in individual-major format (Note: may use huge RAM)."<<endl;
    _fid.clear();
    _pid.clear();
    _geno_dose.clear();
    
    vector<int> kp_it;
    while(1){
        bool kp_flag=true;
        zinf.getline(buf, MAX_LINE_LENGTH, '\n');
        stringstream ss(buf);
        if(!(ss>>str_buf)) break;
        int ibuf=StrFunc::split_string(str_buf, vs_buf, ">");
        if(ibuf>1){
            if(vs_buf[0].empty()) throw("Error: family ID of the individual ["+str_buf+"] is missing.");
            else vs_buf[0].erase(vs_buf[0].end()-1);
        }
        else if(ibuf==1) vs_buf.push_back(vs_buf[0]);
        else break;
        id_buf=vs_buf[0]+":"+vs_buf[1];
        if(kp_indi_flag && kp_id_map.find(id_buf)==kp_id_map.end()) kp_flag=false;
        if(kp_flag && blup_indi_flag && blup_id_map.find(id_buf)==blup_id_map.end()) kp_flag=false;
        if(kp_flag && rm_indi_flag && rm_id_map.find(id_buf)!=rm_id_map.end()) kp_flag=false;
        if(kp_flag){
            kp_it.push_back(1);
            _fid.push_back(vs_buf[0]);
            _pid.push_back(vs_buf[1]);
            kept_id.push_back(id_buf);
        }
        else kp_it.push_back(0);
        if(zinf.fail() || !zinf.good()) break;
    }
    zinf.clear();
    zinf.close();
    cout<<"(Imputed dosage data for "<<kp_it.size()<<" individuals detected)."<<endl;
    _indi_num=_fid.size();
    
    zinf.open(zdosefile.c_str());
    _geno_dose.resize(_indi_num);
    for(line=0; line<_indi_num; line++) _geno_dose[line].resize(_include.size());
    for(line=0, k=0; line<kp_it.size(); line++){
        zinf.getline(buf, MAX_LINE_LENGTH, '\n');
        if(kp_it[line]==0) continue;
        stringstream ss(buf);
        if(!(ss>>str_buf)) break;
        if(!(ss>>str_buf)) break;
        for(i=0, j=0; i<_snp_num; i++){
			ss>>str_buf;
			f_buf=atof(str_buf.c_str());
			if(str_buf=="X" || str_buf=="NA"){
				if(!missing){
					cout<<"Warning: missing values detected in the dosage data."<<endl;
					missing=true;
				}
				f_buf=1e6;
			}
            if(rsnp[i]){ _geno_dose[k][j]=(f_buf); j++; }
        }
        k++;
    }
    zinf.clear();
    zinf.close();
    
    cout<<"Imputed dosage data for "<<kept_id.size()<<" individuals are included from ["<<zdosefile<<"]."<<endl;
    _fa_id.resize(_indi_num);
    _mo_id.resize(_indi_num);
    _sex.resize(_indi_num);
    _pheno.resize(_indi_num);
	for(i=0; i<_indi_num; i++){
		_fa_id[i]=_mo_id[i]="0";
		_sex[i]=-9;
		_pheno[i]=-9;
	}
    
 	// initialize keep
    init_keep();
    update_id_map_kp(kept_id, _id_map, _keep);
    if(_keep.size()==0) throw("Error: No individual is retained for analysis.");
    
    if(blup_indi_flag) read_indi_blup(blup_indi_file);
    
	// update data
	update_bim(rsnp);
}

void gcta::read_imp_info_beagle(string zinfofile)
{
    _dosage_flag=true;

    const int MAX_LINE_LENGTH = 1000;
    char buf[MAX_LINE_LENGTH];
    string str_buf, errmsg="Error: Reading SNP summary information filed? Please check the format of ["+zinfofile+"].";

    string c_buf;
    int i_buf;
    double f_buf=0.0;
    gzifstream zinf;
    zinf.open( zinfofile.c_str() );
    if(! zinf.is_open()) throw("Error: can not open the file ["+zinfofile+"] to read.");
    cout<<"Reading summary information of the imputed SNPs (BEAGLE) ..."<<endl;
    zinf.getline(buf, MAX_LINE_LENGTH, '\n'); // skip the header
    while(1){
        zinf.getline(buf, MAX_LINE_LENGTH, '\n');
        if(zinf.fail() || !zinf.good()) break;
        stringstream ss(buf);
        string nerr=errmsg+"\nError line: "+ss.str();
        if(!(ss>>i_buf)) throw(nerr);
        _chr.push_back(i_buf);
        if(!(ss>>str_buf)) throw(nerr);
        _snp_name.push_back(str_buf);
        if(!(ss>>i_buf)) throw(nerr);
        _bp.push_back(i_buf);
        if(!(ss>>c_buf)) throw(nerr);
        _allele1.push_back(c_buf);
        if(!(ss>>c_buf)) throw(nerr);
        _allele2.push_back(c_buf);
        if(!(ss>>str_buf)) throw(nerr);
        if(!(ss>>str_buf)) throw(nerr);
        if(!(ss>>str_buf)) throw(nerr);
        if(!(ss>>f_buf)) throw(nerr);
        if(!(ss>>f_buf)) throw(nerr);
        if(!(ss>>f_buf)) throw(nerr);
        _impRsq.push_back(f_buf);
        if(!(ss>>f_buf)) throw(nerr);
        if(!(ss>>f_buf)) throw(nerr);
        if(ss>>f_buf) throw(nerr);
    }
    zinf.clear();
    zinf.close();
    _snp_num=_snp_name.size();
    cout<<_snp_num<<" SNPs to be included from ["+zinfofile+"]."<<endl;
    _genet_dst.resize(_snp_num);
    _ref_A=_allele1;
    _other_A=_allele2;

 	// Initialize _include
	init_include();
}

void gcta::read_imp_dose_beagle(string zdosefile, string kp_indi_file, string rm_indi_file, string blup_indi_file)
{
	if(_include.size()==0) throw("Error: No SNP is retained for analysis.");
    int i=0, j=0;
    vector<int> rsnp;
    get_rsnp(rsnp);

    const int MAX_LINE_LENGTH=10000000;
    char buf[MAX_LINE_LENGTH];
    string str_buf;

    gzifstream zinf;
    zinf.open( zdosefile.c_str() );
    if(! zinf.is_open()) throw("Error: can not open the file ["+zdosefile+"] to read.");
    cout<<"Reading imputed dosage scores (BEAGLE output) ..."<<endl;
    zinf.getline(buf, MAX_LINE_LENGTH, '\n');
    stringstream ss(buf);
    for(i=0; i<3; i++) ss>>str_buf;
    while(ss>>str_buf){ _fid.push_back(str_buf); }
    _pid=_fid;
    _indi_num=_fid.size();
    _fa_id.resize(_indi_num);
    _mo_id.resize(_indi_num);
    _sex.resize(_indi_num);
    _pheno.resize(_indi_num);
    cout<<_indi_num<<" individuals to be included from ["+zdosefile+"]."<<endl;
    init_keep();
    if(!kp_indi_file.empty()) keep_indi(kp_indi_file);
    if(!blup_indi_file.empty()) read_indi_blup(blup_indi_file);
    if(!rm_indi_file.empty()) remove_indi(rm_indi_file);

    _geno_dose.resize(_keep.size());
    for(i=0; i<_keep.size(); i++) _geno_dose[i].resize(_include.size());

    vector<int> rindi;
    get_rindi(rindi);

    int line=0;
    int k=0;
    double d_buf=0.0;
    while(1){
        zinf.getline(buf, MAX_LINE_LENGTH, '\n');
        if(zinf.fail() || !zinf.good()) break;
        if(!rsnp[line++]) continue;
        stringstream ss(buf);
        ss>>str_buf;
        if(str_buf!=_snp_name[line-1]){
            stringstream errmsg;
            errmsg<<"Error: the "<<line<<" th SNP ["+_snp_name[line-1]+"] in the summary file doesn't match to that in the dosage file."<<endl;
            throw(errmsg.str());
        }
        ss>>str_buf>>str_buf;
        for(i=0, j=0; i<_indi_num; i++){
            ss>>d_buf;
            if(rindi[i]){ _geno_dose[j][k]=d_buf; j++; }
        }
        k++;
    }
    zinf.clear();
    zinf.close();
}

void gcta::save_plink()
{
	if(_dosage_flag) dose2bed();
    save_famfile();
    save_bimfile();
    save_bedfile();
}

void gcta::save_bedfile()
{
    int i=0, pos=0, j=0;
    string OutBedFile=_out+".bed";
	fstream OutBed(OutBedFile.c_str(), ios::out|ios::binary);
	if(!OutBed) throw("Error: can not open the file ["+OutBedFile+"] to write.");
	cout<<"Writing genotypes to PLINK BED file ["+OutBedFile+"] ..."<<endl;
	bitset<8> b;
	char ch[1];
    b.reset();
    b.set(2);  b.set(3);  b.set(5);  b.set(6);
    ch[0] = (char)b.to_ulong();
    OutBed.write(ch,1);
    b.reset();
    b.set(0);  b.set(1);  b.set(3);  b.set(4);
    ch[0] = (char)b.to_ulong();
    OutBed.write(ch,1);
    b.reset();
    b.set(0);
    ch[0] = (char)b.to_ulong();
    OutBed.write(ch,1);
    for(i=0; i<_include.size(); i++){
        pos=0;
        b.reset();
        for(j=0; j<_keep.size(); j++){
            b[pos++]=(!_snp_2[_include[i]][_keep[j]]);
            b[pos++]=(!_snp_1[_include[i]][_keep[j]]);
            if(pos>7 || j==_keep.size()-1){
                ch[0]=(char)b.to_ulong();
                OutBed.write(ch,1);
                pos=0;
                b.reset();
            }
        }
    }
    OutBed.close();
}

void gcta::save_famfile()
{
    string famfile=_out+".fam";
	ofstream Fam(famfile.c_str());
	if(!Fam) throw("Error: can not open the fam file "+famfile+" to save!");
	cout<<"Writing PLINK FAM file to ["+famfile+"] ..."<<endl;
	int i=0;
	for(i=0; i<_keep.size(); i++){
		Fam<<_fid[_keep[i]]<<"\t"<<_pid[_keep[i]]<<"\t"<<_fa_id[_keep[i]]<<"\t"<<_mo_id[_keep[i]]<<"\t"<<_sex[_keep[i]]<<"\t"<<_pheno[_keep[i]]<<endl;
	}
	Fam.close();
	cout<<_keep.size()<<" individuals to be saved to ["+famfile+"]."<<endl;
}

void gcta::save_bimfile()
{
	int i=0;
	string bimfile=_out+".bim";
	ofstream Bim(bimfile.c_str());
	if(!Bim) throw("Error: can not open the file ["+bimfile+"] to write.");
	cout<<"Writing PLINK bim file to ["+bimfile+"] ..."<<endl;
	for(i=0; i<_include.size(); i++){
		Bim<<_chr[_include[i]]<<"\t"<<_snp_name[_include[i]]<<"\t"<<_genet_dst[_include[i]]<<"\t"<<_bp[_include[i]]<<"\t"<<_allele1[_include[i]]<<"\t"<<_allele2[_include[i]]<<endl;
	}
	Bim.close();
	cout<<_include.size()<<" SNPs to be saved to ["+bimfile+"]."<<endl;
}

void gcta::dose2bed()
{
	int i=0, j=0;
	double d_buf=0.0;
	
	cout<<"Converting dosage data into PLINK binary PED format ... "<<endl;
    _snp_1.resize(_snp_num);
    _snp_2.resize(_snp_num);
    for(i=0; i<_include.size(); i++){
        _snp_1[i].resize(_indi_num);
        _snp_2[i].resize(_indi_num);
		for(j=0; j<_keep.size(); j++){
			d_buf=_geno_dose[_keep[j]][_include[i]];
            if(d_buf>1e5){
				_snp_2[_include[i]][_keep[j]]=false;
                _snp_1[_include[i]][_keep[j]]=true;
            }
			else if(d_buf>=1.5) _snp_1[_include[i]][_keep[j]]=_snp_2[_include[i]][_keep[j]]=true;
			else if(d_buf>0.5){
				_snp_2[_include[i]][_keep[j]]=true;
				_snp_1[_include[i]][_keep[j]]=false;
			}
			else if(d_buf<=0.5) _snp_1[_include[i]][_keep[j]]=_snp_2[_include[i]][_keep[j]]=false;
		}
    }
}

void gcta::update_id_map_kp(const vector<string> &id_list, map<string, int> &id_map, vector<int> &keep)
{
    int i=0;
    map<string, int> id_map_buf(id_map);
    for(i=0; i<id_list.size(); i++) id_map_buf.erase(id_list[i]);
    map<string, int>::iterator iter;
    for(iter=id_map_buf.begin(); iter!=id_map_buf.end(); iter++) id_map.erase(iter->first);

    keep.clear();
    for(iter=id_map.begin(); iter!=id_map.end(); iter++) keep.push_back(iter->second);
    stable_sort(keep.begin(), keep.end());
}

void gcta::update_id_map_rm(const vector<string> &id_list, map<string, int> &id_map, vector<int> &keep)
{
    int i=0;
    for(i=0; i<id_list.size(); i++) id_map.erase(id_list[i]);

    keep.clear();
    map<string, int>::iterator iter;
    for(iter=id_map.begin(); iter!=id_map.end(); iter++) keep.push_back(iter->second);
    stable_sort(keep.begin(), keep.end());
}

void gcta::read_snplist(string snplistfile, vector<string> &snplist, string msg)
{
	// Read snplist file
	snplist.clear();
	string StrBuf;
	ifstream i_snplist(snplistfile.c_str());
	if(!i_snplist) throw("Error: can not open the file ["+snplistfile+"] to read.");
	cout<<"Reading a list of "<<msg<<" from ["+snplistfile+"]."<<endl;
	while(i_snplist>>StrBuf){
	    snplist.push_back(StrBuf);
	    getline(i_snplist, StrBuf);
	}
	i_snplist.close();
}

void gcta::extract_snp(string snplistfile)
{
    vector<string> snplist;
    read_snplist(snplistfile, snplist);
    update_id_map_kp(snplist, _snp_name_map, _include);
    cout<<_include.size()<<" SNPs are extracted from ["+snplistfile+"]."<<endl;
}

void gcta::extract_single_snp(string snpname)
{
    vector<string> snplist;
    snplist.push_back(snpname);
    update_id_map_kp(snplist, _snp_name_map, _include);
    if(_include.empty()) throw("Error: can not find the SNP ["+snpname+"] in the data.");
    else cout<<"Only the SNP ["+snpname+"] is included in the analysis."<<endl;
}

void gcta::exclude_snp(string snplistfile)
{
    vector<string> snplist;
    read_snplist(snplistfile, snplist);
    int prev_size=_include.size();
    update_id_map_rm(snplist, _snp_name_map, _include);
    cout<<prev_size-_include.size()<<" SNPs are excluded from ["+snplistfile+"] and there are "<<_include.size()<<" SNPs remaining."<<endl;
}

void gcta::exclude_single_snp(string snpname)
{
    vector<string> snplist;
    snplist.push_back(snpname);
    int include_size=_include.size();
    update_id_map_rm(snplist, _snp_name_map, _include);
    if(_include.size()==include_size) throw("Error: can not find the SNP ["+snpname+"] in the data.");
    else cout<<"The SNP["+snpname+"] has been excluded from the analysis."<<endl;
}

void gcta::extract_chr(int chr_start, int chr_end)
{
    map<string, int> id_map_buf(_snp_name_map);
    map<string, int>::iterator iter, end=id_map_buf.end();
    _snp_name_map.clear();
    _include.clear();
    for(iter=id_map_buf.begin(); iter!=end; iter++){
        if(_chr[iter->second]>=chr_start && _chr[iter->second]<=chr_end){
            _snp_name_map.insert(*iter);
            _include.push_back(iter->second);
        }
    }
    stable_sort(_include.begin(), _include.end());
    if(chr_start!=chr_end) cout<<_include.size()<<" SNPs from chromosome "<<chr_start<<" to chromosome "<<chr_end<<" are included in the analysis."<<endl;
    else cout<<_include.size()<<" SNPs on chromosome "<<chr_start<<" are included in the analysis."<<endl;
}

void gcta::filter_snp_maf(double maf)
{
    if(_mu.empty()) calcu_mu();

    cout<<"Pruning SNPs with MAF > "<<maf<<" ..."<<endl;
    map<string, int> id_map_buf(_snp_name_map);
    map<string, int>::iterator iter, end=id_map_buf.end();
    int prev_size=_include.size();
    double fbuf=0.0;
    _include.clear();
    _snp_name_map.clear();
    for(iter=id_map_buf.begin(); iter!=end; iter++){
        fbuf=_mu[iter->second]*0.5;
        if(fbuf<=maf || (1.0-fbuf)<=maf) continue;
        _snp_name_map.insert(*iter);
        _include.push_back(iter->second);
    }
	if(_include.size()==0) throw("Error: No SNP is retained for analysis.");
	else{
		stable_sort(_include.begin(), _include.end());
		cout<<"After pruning SNPs with MAF > "<<maf<<", there are "<<_include.size()<<" SNPs ("<<prev_size-_include.size()<<" SNPs with MAF < "<<maf<<")."<<endl;
	}
}

void gcta::filter_snp_max_maf(double max_maf)
{
    if(_mu.empty()) calcu_mu();

    cout<<"Pruning SNPs with MAF < "<<max_maf<<" ..."<<endl;
    map<string, int> id_map_buf(_snp_name_map);
    map<string, int>::iterator iter, end=id_map_buf.end();
    int prev_size=_include.size();
    double fbuf=0.0;
    _include.clear();
    _snp_name_map.clear();
    for(iter=id_map_buf.begin(); iter!=end; iter++){
        fbuf=_mu[iter->second]*0.5;
        if(fbuf>max_maf && 1.0-fbuf>max_maf) continue;
        _snp_name_map.insert(*iter);
        _include.push_back(iter->second);
    }
	if(_include.size()==0) throw("Error: No SNP is retained for analysis.");
	else{
		stable_sort(_include.begin(), _include.end());
		cout<<"After pruning SNPs with MAF < "<<max_maf<<", there are "<<_include.size()<<" SNPs ("<<prev_size-_include.size()<<" SNPs with MAF > "<<max_maf<<")."<<endl;
	}
}

void gcta::filter_impRsq(double rsq_cutoff)
{
    if(_impRsq.empty()) cout<<"Warning: the option --imput-rsq is inactive because GCTA can't find the imputation quality scores for the SNPs. Use the option --update-imput-rsq to input the imputation quality scores."<<endl;
    cout<<"Pruning SNPs with imputation Rsq > "<<rsq_cutoff<<" ..."<<endl;
    map<string, int> id_map_buf(_snp_name_map);
    map<string, int>::iterator iter, end=id_map_buf.end();
    int prev_size=_include.size();
    _include.clear();
    _snp_name_map.clear();
    for(iter=id_map_buf.begin(); iter!=end; iter++){
        if(_impRsq[iter->second]<rsq_cutoff) continue;
        _snp_name_map.insert(*iter);
        _include.push_back(iter->second);
    }
	if(_include.size()==0) throw("Error: No SNP is retained for analysis.");
	else{
		stable_sort(_include.begin(), _include.end());
		cout<<"After pruning for imputation Rsq > "<<rsq_cutoff<<", there are "<<_include.size()<<" SNPs ("<<prev_size-_include.size()<<" SNPs with imputation Rsq < "<<rsq_cutoff<<")."<<endl;
	}
}

void gcta::read_indi_list(string indi_list_file, vector<string> &indi_list)
{
	ifstream i_indi_list(indi_list_file.c_str());
	if(!i_indi_list) throw("Error: can not open the file ["+indi_list_file+"] to read.");
	string str_buf, id_buf;
    indi_list.clear();
	while(i_indi_list){
	    i_indi_list>>str_buf;
	    if(i_indi_list.eof()) break;
		id_buf=str_buf+":";
	    i_indi_list>>str_buf;
		id_buf+=str_buf;
		indi_list.push_back(id_buf);
		getline(i_indi_list, str_buf);
	}
	i_indi_list.close();
}

void gcta::keep_indi(string indi_list_file)
{
    vector<string> indi_list;
    read_indi_list(indi_list_file, indi_list);
    update_id_map_kp(indi_list, _id_map, _keep);
    cout<<_keep.size()<<" individuals are kept from ["+indi_list_file+"]."<<endl;
}

void gcta::remove_indi(string indi_list_file)
{
    vector<string> indi_list;
    read_indi_list(indi_list_file, indi_list);
    int prev_size=_keep.size();
    update_id_map_rm(indi_list, _id_map, _keep);
    cout<<prev_size-_keep.size()<<" individuals are removed from ["+indi_list_file+"] and there are "<<_keep.size()<<" individuals remaining."<<endl;
}

void gcta::update_sex(string sex_file)
{
    ifstream isex(sex_file.c_str());
    if(!isex) throw("Error: can not open the file ["+sex_file+"] to read.");
    int sex_buf=0, icount=0;
    string str_buf, fid, pid;
	cout<<"Reading sex information from ["+sex_file+"]."<<endl;
    map<string, int>::iterator iter, End=_id_map.end();
    _sex.clear();
    _sex.resize(_indi_num);
    vector<int> confirm(_indi_num);
	while(isex){
		isex>>fid;
		if(isex.eof()) break;
		isex>>pid;
		isex>>str_buf;
		if(str_buf!="1" && str_buf!="2" && str_buf!="M" && str_buf!="F") throw("Error: unrecognized sex code: \""+fid+" "+pid+" "+str_buf+"\" in ["+sex_file+"].");
		iter=_id_map.find(fid+":"+pid);
		if(iter!=End){
		    if(str_buf=="M" || str_buf=="1") _sex[iter->second]=1;
		    else if(str_buf=="F" || str_buf=="2") _sex[iter->second]=2;
		    confirm[iter->second]=1;
		    icount++;
		}
		getline(isex, str_buf);
	}
    isex.close();

    for(int i=0; i<_keep.size(); i++){
        if(confirm[_keep[i]]!=1) throw("Error: sex information for all of the included individuals should be updated.");
    }
	cout<<"Sex information for "<<icount<<" individuals are update from ["+sex_file+"]."<<endl;
}

void gcta::update_ref_A(string ref_A_file)
{
    ifstream i_ref_A(ref_A_file.c_str());
    if(!i_ref_A) throw("Error: can not open the file ["+ref_A_file+"] to read.");
    int i=0;
    string str_buf, ref_A_buf;
	cout<<"Reading reference alleles of SNPs from ["+ref_A_file+"]."<<endl;
    map<string, int>::iterator iter, End=_snp_name_map.end();
    int icount=0;
	while(i_ref_A){
		i_ref_A>>str_buf;
		if(i_ref_A.eof()) break;
		iter=_snp_name_map.find(str_buf);
		i_ref_A>>ref_A_buf;
		if(iter!=End){
            if(ref_A_buf==_allele1[iter->second]){
                _ref_A[iter->second]=_allele1[iter->second]; 
                _other_A[iter->second]=_allele2[iter->second];
            }
            else if(ref_A_buf==_allele2[iter->second]){
                _ref_A[iter->second]=_allele2[iter->second]; 
                _other_A[iter->second]=_allele1[iter->second];
            }
            else throw("Error: invalid reference allele for SNP \""+_snp_name[iter->second]+"\".");
		    icount++;
		}
		getline(i_ref_A, str_buf);
	}
    i_ref_A.close();
	cout<<"Reference alleles of "<<icount<<" SNPs are update from ["+ref_A_file+"]."<<endl;
    if(icount!=_snp_num) cout<<"Warning: reference alleles of "<<_snp_num-icount<<" SNPs have not been updated."<<endl;
}

void gcta::calcu_mu(bool ssq_flag)
{
	int i=0, j=0;

    vector<double> auto_fac(_keep.size()), xfac(_keep.size()), fac(_keep.size());
    for(i=0; i<_keep.size(); i++){
        auto_fac[i]=1.0;
        if(_sex[_keep[i]]==1) xfac[i]=0.5;
        else if(_sex[_keep[i]]==2) xfac[i]=1.0;
        fac[i]=0.5;
    }

    cout<<"Calculating allele frequencies ..."<<endl;
    _mu.clear();
	_mu.resize(_snp_num);
	
	#pragma omp parallel for
    for(j=0; j<_include.size(); j++){
        if(_chr[_include[j]]<(_autosome_num+1)) mu_func(j, auto_fac);
        else if(_chr[_include[j]]==(_autosome_num+1)) mu_func(j, xfac);
        else mu_func(j, fac);
    }
}

void gcta::mu_func(int j, vector<double> &fac)
{
    int i=0;
    double fcount=0.0, f_buf=0.0;
    if(_dosage_flag){
        for(i=0; i<_keep.size(); i++){
            if(_geno_dose[_keep[i]][_include[j]]<1e5){
                _mu[_include[j]]+=fac[i]*_geno_dose[_keep[i]][_include[j]];
                fcount+=fac[i];
            }
        }
    }
    else{
        for(i=0; i<_keep.size(); i++){
            if(!_snp_1[_include[j]][_keep[i]] || _snp_2[_include[j]][_keep[i]]){
                f_buf=(_snp_1[_include[j]][_keep[i]]+_snp_2[_include[j]][_keep[i]]);
                if(_allele2[_include[j]]==_ref_A[_include[j]]) f_buf=2.0-f_buf;
                _mu[_include[j]]+=fac[i]*f_buf;
                fcount+=fac[i];
            }
        }
    }

    if(fcount>0.0)_mu[_include[j]]/=fcount;
}

void gcta::update_impRsq(string zinfofile)
{
    ifstream iRsq(zinfofile.c_str());
    if(!iRsq) throw("Error: can not open the file ["+zinfofile+"] to read.");

    string snp_name_buf, str_buf;
    double fbuf=0.0;
    cout<<"Reading imputation Rsq of the SNPs from ["+zinfofile+"]."<<endl;
    _impRsq.clear();
    _impRsq.resize(_snp_num, 0.0);
    map<string, int>::iterator iter, End=_snp_name_map.end();
    int icount=0;
    while(iRsq){
        iRsq>>snp_name_buf;
		if(iRsq.eof()) break;
		iter=_snp_name_map.find(snp_name_buf);
		iRsq>>str_buf;
        fbuf=atof(str_buf.c_str());
		if(iter!=End){
		    if(fbuf>2.0 || fbuf<0.0) throw("Error: invalid value of imputation Rsq for the SNP "+snp_name_buf+".");
            _impRsq[iter->second]=fbuf;
		    icount++;
		}
		getline(iRsq, str_buf);
    }
    iRsq.close();
    
	cout<<"Imputation Rsq of "<<icount<<" SNPs are update from ["+zinfofile+"]."<<endl;
    if(icount!=_snp_num) cout<<"Warning: imputation Rsq of "<<_snp_num-icount<<" SNPs have not been updated."<<endl;
}

void gcta::update_freq(string freq)
{
    ifstream ifreq(freq.c_str());
    if(!ifreq) throw("Error: can not open the file ["+freq+"] to read.");
    int i=0;
    string ref_A_buf;
    double fbuf=0.0;
    string snp_name_buf, str_buf;
	cout<<"Reading allele frequencies of the SNPs from ["+freq+"]."<<endl;
    map<string, int>::iterator iter, End=_snp_name_map.end();
    _mu.clear();
    _mu.resize(_snp_num, 0.0);
    int icount=0;
	while(ifreq){
		ifreq>>snp_name_buf;
		if(ifreq.eof()) break;
		iter=_snp_name_map.find(snp_name_buf);
		ifreq>>ref_A_buf;
        ifreq>>str_buf;
		fbuf=atof(str_buf.c_str());
		if(iter!=End){
		    if(fbuf>1.0 || fbuf<0.0) throw("Error: invalid value of allele frequency for the SNP "+snp_name_buf+".");
            if(ref_A_buf!=_allele1[iter->second] && ref_A_buf!=_allele2[iter->second]){
                throw("Invalid allele type \""+ref_A_buf+"\" for the SNP "+_snp_name[iter->second]+".");
            }
		    if(ref_A_buf==_ref_A[iter->second]) _mu[iter->second]=fbuf*2.0;
		    else _mu[iter->second]=(1.0-fbuf)*2.0;
		    icount++;
		}
		getline(ifreq, str_buf);
	}
    ifreq.close();

	cout<<"Allele frequencies of "<<icount<<" SNPs are update from ["+freq+"]."<<endl;
    if(icount!=_snp_num) cout<<"Warning: allele frequencies of "<<_snp_num-icount<<" SNPs have not been updated."<<endl;
}

void gcta::save_freq(bool ssq_flag)
{
    if(_mu.empty()) calcu_mu(ssq_flag);
    string save_freq=_out+".freq";
    ofstream ofreq(save_freq.c_str());
    if(!ofreq) throw("Error: can not open the file ["+save_freq+"] to write.");
    int i=0;
	cout<<"Writing allele frequencies of "<<_include.size()<<" SNPs to ["+save_freq+"]."<<endl;
    for(i=0; i<_include.size(); i++){
        ofreq<<_snp_name[_include[i]]<<"\t"<<_ref_A[_include[i]]<<"\t"<<setprecision(15)<<_mu[_include[i]]*0.5;
//        if(ssq_flag) ofreq<<"\t"<<_ssq[_include[i]]<<"\t"<<_w[_include[i]];
        ofreq<<endl;
    }
    ofreq.close();
	cout<<"Allele frequencies of "<<_include.size()<<" SNPs have been saved in the file ["+save_freq+"]."<<endl;
}

void gcta::read_indi_blup(string blup_indi_file)
{
    vector< vector<string> > g_buf;
	ifstream i_indi_blup(blup_indi_file.c_str());
	if(!i_indi_blup) throw("Error: can not open the file ["+blup_indi_file+"] to read.");
	string str_buf, id_buf;
	vector<string> id, vs_buf;
	int i=0, j=0, k=0, col_num=0;
	while(i_indi_blup){
	    i_indi_blup>>str_buf;
	    if(i_indi_blup.eof()) break;
		id_buf=str_buf+":";
	    i_indi_blup>>str_buf;
	    id_buf+=str_buf;
		getline(i_indi_blup, str_buf);
		col_num=StrFunc::split_string(str_buf, vs_buf);
		if(col_num<1) continue;
		id.push_back(id_buf);
		g_buf.push_back(vs_buf);
	}
	i_indi_blup.close();

	update_id_map_kp(id, _id_map, _keep);
    map<string, int> uni_id_map;
    map<string, int>::iterator iter;
	for(i=0; i<_keep.size(); i++) uni_id_map.insert(pair<string,int>(_fid[_keep[i]]+":"+_pid[_keep[i]], i));
	_varcmp_Py.setZero(_keep.size(), col_num/2);
	for(i=0; i<id.size(); i++){
	    iter=uni_id_map.find(id[i]);
	    if(iter==uni_id_map.end()) continue;
	    for(j=0, k=0; j<col_num; j+=2, k++) _varcmp_Py(iter->second,k)=atof(g_buf[i][j].c_str());
	}
	cout<<"BLUP solution to the total genetic effects for "<<_keep.size()<<" individuals have been read from ["+blup_indi_file+"]."<<endl;	
}

void gcta::make_XMat(vector< vector<float> > &X, bool miss_with_mu)
{
    if(_mu.empty() && miss_with_mu) calcu_mu();

	cout<<"Recoding genotypes (individual major mode) ..."<<endl;
	int i=0, j=0;
	X.clear();
	X.resize(_keep.size());
	
	for(i=0; i<_keep.size(); i++){
	    X[i].resize(_include.size());
        bool need2fill=false;
		if(_dosage_flag){
            for(j=0; j<_include.size(); j++){
                if(_geno_dose[_keep[i]][_include[j]]<1e5){
                    if(_allele1[_include[j]]==_ref_A[_include[j]]) X[i][j]=_geno_dose[_keep[i]][_include[j]];
                    else X[i][j]=2.0-_geno_dose[_keep[i]][_include[j]];
                }
                else{ X[i][j]=1e6; need2fill=true; }
            }
            _geno_dose[i].clear();
		}
		else{
            for(j=0; j<_include.size(); j++){
                if(!_snp_1[_include[j]][_keep[i]] || _snp_2[_include[j]][_keep[i]]){
                    if(_allele1[_include[j]]==_ref_A[_include[j]]) X[i][j]=_snp_1[_include[j]][_keep[i]]+_snp_2[_include[j]][_keep[i]];
                    else X[i][j]=2.0-(_snp_1[_include[j]][_keep[i]]+_snp_2[_include[j]][_keep[i]]);
                }
                else{ X[i][j]=1e6; need2fill=true; }
            }
		}
        // Fill the missing genotype with the mean of x (2p)
        for(j=0; j<_include.size() && miss_with_mu && need2fill; j++){
            if(X[i][j]>1e5) X[i][j]=_mu[_include[j]];
        }
	}
}

void gcta::std_XMat(vector< vector<float> > &X, vector<double> &sd_SNP, bool grm_xchr_flag, bool divid_by_std)
{
	if(_mu.empty()) calcu_mu();

    int i=0, j=0;
	sd_SNP.clear();
	sd_SNP.resize(_include.size()); // SD of each SNP, sqrt(2pq)
	if(_dosage_flag){
        for(j=0; j<_include.size(); j++){
            for(i=0; i<_keep.size(); i++) sd_SNP[j]+=(X[i][j]-_mu[_include[j]])*(X[i][j]-_mu[_include[j]]);
            sd_SNP[j]/=(_keep.size()-1.0);
        }
	}
	else{
        for(j=0; j<_include.size(); j++) sd_SNP[j]=_mu[_include[j]]*(1.0-0.5*_mu[_include[j]]);
	}
    for(j=0; j<_include.size(); j++){
        if(fabs(sd_SNP[j])<1.0e-50) sd_SNP[j]=0.0;
        else sd_SNP[j]=sqrt(1.0/sd_SNP[j]);
    }
    for(i=0; i<_keep.size(); i++){
         for(j=0; j<_include.size(); j++){
            if(X[i][j]<1e5){
                X[i][j]-=_mu[_include[j]];
                if(divid_by_std) X[i][j]*=sd_SNP[j];
            }
         }
	}

	if(!grm_xchr_flag) return;
	// for the X-chromosome
	check_sex();
	double f_buf=sqrt(0.5);
    for(i=0; i<_keep.size(); i++){
        if(_sex[_keep[i]]==1){
            for(j=0; j<_include.size(); j++){
                if(X[i][j]<1e5) X[i][j]*=f_buf;
            }
        }
	}
}

// SNP mode: each vector is the genotype of a SNP with all of individuals
void gcta::make_XMat_SNPs(vector< vector<float> > &X, bool miss_with_mu)
{
    if(_mu.empty() && miss_with_mu) calcu_mu();

	cout<<"Recoding genotypes (SNP major mode) ..."<<endl;
	int i=0, j=0;
	X.clear();
	X.resize(_include.size());
	for(i=0; i<_include.size(); i++) X[i].resize(_keep.size());
	for(i=0; i<_keep.size(); i++){
        bool need2fill=false;
	    if(_dosage_flag){
            for(j=0; j<_include.size(); j++){
                if(_geno_dose[_keep[i]][_include[j]]<1e5){
                    if(_allele1[_include[j]]==_ref_A[_include[j]]) X[j][i]=_geno_dose[_keep[i]][_include[j]];
                    else X[j][i]=2.0-_geno_dose[_keep[i]][_include[j]];
                }
                else{ X[j][i]=1e6; need2fill=true; }
            }
            _geno_dose[i].clear();
		}
		else{
            for(j=0; j<_include.size(); j++){
                if(!_snp_1[_include[j]][_keep[i]] || _snp_2[_include[j]][_keep[i]]){
                    if(_allele1[_include[j]]==_ref_A[_include[j]]) X[j][i]=(_snp_1[_include[j]][_keep[i]]+_snp_2[_include[j]][_keep[i]]);
                    else X[j][i]=2.0-(_snp_1[_include[j]][_keep[i]]+_snp_2[_include[j]][_keep[i]]);
                }
                else{ X[j][i]=1e6; need2fill=true; }
            }
		}
        // Fill the missing genotype with the mean of x (2p)
        for(j=0; j<_include.size() && miss_with_mu && need2fill; j++){
            if(X[j][i]>1e5) X[j][i]=_mu[_include[j]];
        }
	}
}

/*
// SNP major mode
void gcta::std_XMat_SNPs(vector< vector<float> > &X, vector<double> &sd_SNP, bool grm_xchr_flag)
{
	if(_mu.empty()) calcu_mu();

    int i=0, j=0;
	sd_SNP.clear();
	sd_SNP.resize(_include.size()); // SD of each SNP, sqrt(2pq)
	if(_dosage_flag){
        for(j=0; j<_include.size(); j++){
            for(i=0; i<_keep.size(); i++) sd_SNP[j]+=(X[j][i]-_mu[_include[j]])*(X[j][i]-_mu[_include[j]]);
            sd_SNP[j]/=(_keep.size()-1.0);
        }
	}
	else{
        for(j=0; j<_include.size(); j++) sd_SNP[j]=_mu[_include[j]]*(1.0-0.5*_mu[_include[j]]);
	}
    for(j=0; j<_include.size(); j++){
        if(fabs(sd_SNP[j])<1.0e-50) sd_SNP[j]=0.0;
        else sd_SNP[j]=sqrt(1.0/sd_SNP[j]);
    }
    for(j=0; j<_include.size(); j++){
        for(i=0; i<_keep.size(); i++){
            if(X[j][i]<1e5) X[j][i]=(X[j][i]-_mu[_include[j]])*sd_SNP[j];
        }
	}

	if(!grm_xchr_flag) return;
	// for the X-chromosome
	check_sex();
	double f_buf=sqrt(0.5);
    for(i=0; i<_keep.size(); i++){
        if(_sex[_keep[i]]==1){
            for(j=0; j<_include.size(); j++){
                if(X[j][i]<1e5) X[j][i]*=f_buf;
            }
        }
	}
}
*/

void gcta::makex_eigenVector(int j, eigenVector &x, bool resize, bool minus_2p)
{
    int i=0;
    if(resize) x.resize(_keep.size());
    for(i=0; i<_keep.size(); i++){
        if(!_snp_1[_include[j]][_keep[i]] || _snp_2[_include[j]][_keep[i]]){
            if(_allele1[_include[j]]==_ref_A[_include[j]]) x[i]=(_snp_1[_include[j]][_keep[i]]+_snp_2[_include[j]][_keep[i]]);
            else x[i]=2.0-(_snp_1[_include[j]][_keep[i]]+_snp_2[_include[j]][_keep[i]]);
        }
        else x[i]=_mu[_include[j]];
        if(minus_2p) x[i]-=_mu[_include[j]];
    }
}

void gcta::save_XMat(bool miss_with_mu)
{
    if(miss_with_mu && _mu.empty()) calcu_mu();

	// Save matrix X
    string X_zFile=_out+".xmat.gz";
    gzofstream zoutf;
    zoutf.open( X_zFile.c_str() );
    if(!zoutf.is_open()) throw("Error: can not open the file ["+X_zFile+"] to write.");
	cout<<"Saving the recoded genotype matrix to the file ["+X_zFile+"]."<<endl;
    int i=0, j=0;
    zoutf<<"FID IID ";
    for(j=0; j<_include.size(); j++) zoutf<<_snp_name[_include[j]]<<" ";
    zoutf<<endl;
    zoutf<<"Reference Allele ";
    for(j=0; j<_include.size(); j++) zoutf<<_ref_A[_include[j]]<<" ";
    zoutf<<endl;
    for(i=0; i<_keep.size(); i++){
		zoutf<<_fid[_keep[i]]<<' '<<_pid[_keep[i]]<<' ';
        if(_dosage_flag){
            for(j=0; j<_include.size(); j++){
                if(_geno_dose[_keep[i]][_include[j]]<1e5){
                    if(_allele1[_include[j]]==_ref_A[_include[j]]) zoutf<<_geno_dose[_keep[i]][_include[j]]<<' ';
                    else zoutf<<2.0-_geno_dose[_keep[i]][_include[j]]<<' ';
                }
                else{
                    if(miss_with_mu) zoutf<<_mu[_include[j]]<<' ';
                    else zoutf<<"NA ";
                }
            }
		}
		else{
            for(j=0; j<_include.size(); j++){
                if(!_snp_1[_include[j]][_keep[i]] || _snp_2[_include[j]][_keep[i]]){
                    if(_allele1[_include[j]]==_ref_A[_include[j]]) zoutf<<_snp_1[_include[j]][_keep[i]]+_snp_2[_include[j]][_keep[i]]<<' ';
                    else zoutf<<2.0-(_snp_1[_include[j]][_keep[i]]+_snp_2[_include[j]][_keep[i]])<<' ';
                }
                else{
                    if(miss_with_mu) zoutf<<_mu[_include[j]]<<' ';
                    else zoutf<<"NA ";
                }
            }
		}
		zoutf<<endl;
    }
    zoutf.close();
    cout<<"The recoded genotype matrix has been saved in the file ["+X_zFile+"] (in compressed text format) ."<<endl;
}
