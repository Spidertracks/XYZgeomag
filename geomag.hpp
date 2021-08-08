/*
MIT License

Copyright (c) 2019 Nathan Zimmerberg

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


// ../geomag.hpp Generated by python script wmmcodeupdate.py
/** \file
 * \author Nathan Zimmerberg (nhz2@cornell.edu)
 * \date 07 AUG 2021
 * \brief Header-only library to calculate the magnetic field in the International Terrestrial Reference System(ITRS).
 * \details Designed to minimize ram usage for embedded systems.

The data from WMM models is not subject to copyright protection.
Modifications are:
  using ITRS coordinates,
  conversion from nT to T,
  Using unnormalized coefficents genrated by the python script wmmcodeupdate.py
  using a different spherical harmonics calculation, described in sections 3.2.4 and 3.2.5:
    Satellite Orbits Models, Methods and Applications,
      by Oliver Montenbruck and Eberhard Gill 2000
*/
#ifndef GEOMAG_HPP
#define GEOMAG_HPP

#include <math.h>

namespace geomag
{
constexpr int NMAX= 12;//order of the Model
constexpr int NUMCOF= (NMAX+1)*(NMAX+2)/2;//number of coefficents
struct ConstModel{
    float epoch;//decimal year
    float Main_Field_Coeff_C[NUMCOF];
    float Main_Field_Coeff_S[NUMCOF];
    float Secular_Var_Coeff_C[NUMCOF];
    float Secular_Var_Coeff_S[NUMCOF];
    /** Function for indexing the C spherical component n,m at dyear time.*/
    inline float C(int n, int m, float dyear) const{
      int index= (m*(2*NMAX-m+1))/2+n;
      #ifdef PROGMEM
        return pgm_read_float_near(Main_Field_Coeff_C+index)+(dyear-epoch)*pgm_read_float_near(Secular_Var_Coeff_C+index);
      #endif /* PROGMEM */
      return Main_Field_Coeff_C[index]+(dyear-epoch)*Secular_Var_Coeff_C[index];
    }
    /** Function for indexing the S spherical component n,m at dyear time.*/
    inline float S(int n, int m, float dyear) const{
      int index= (m*(2*NMAX-m+1))/2+n;
      #ifdef PROGMEM
        return pgm_read_float_near(Main_Field_Coeff_S+index)+(dyear-epoch)*pgm_read_float_near(Secular_Var_Coeff_S+index);
      #endif /* PROGMEM */
      return Main_Field_Coeff_S[index]+(dyear-epoch)*Secular_Var_Coeff_S[index];
    }
};
//mean radius of  ellipsoid in meters from section 1.2 of the WMM2015 Technical report
constexpr float EARTH_R= 6371200.0f;

typedef struct {
    float x;
    float y;
    float z;
} Vector;

/** Return the magnetic field in International Terrestrial Reference System coordinates, units Tesla.
 INPUT:
    position_itrs(Above the surface of earth): The location where the field is predicted, units m.
    dyear(should be around the epoch of the model): The decimal year, for example 2015.0
    WMM(): Magnetic field model to use.
 */
inline Vector GeoMag(float dyear,Vector position_itrs, const ConstModel& WMM){
    float x= position_itrs.x;
    float y= position_itrs.y;
    float z= position_itrs.z;
    float px= 0;
    float py= 0;
    float pz= 0;
    float rsqrd= x*x+y*y+z*z;
    float temp= EARTH_R/rsqrd;
    float a= x*temp;
    float b= y*temp;
    float f= z*temp;
    float g= EARTH_R*temp;

    int n,m;
    //first m==0 row, just solve for the Vs
    float Vtop= EARTH_R/sqrtf(rsqrd);//V0,0
    float Wtop= 0;//W0,0
    float Vprev= 0;
    float Wprev= 0;
    float Vnm= Vtop;
    float Wnm= Wtop;

    //iterate through all ms
    for ( m = 0; m <= NMAX+1; m++)
    {
        // iterate through all ns
        for (n = m; n <= NMAX+1; n++)
        {
            if (n==m){
                if(m!=0){
                    temp= Vtop;
                    Vtop= (2*m-1)*(a*Vtop-b*Wtop);
                    Wtop= (2*m-1)*(a*Wtop+b*temp);
                    Vprev= 0;
                    Wprev= 0;
                    Vnm= Vtop;
                    Wnm= Wtop;
                }
            }
            else{
                temp= Vnm;
                float invs_temp=1.0f/((float)(n-m));
                Vnm= ((2*n-1)*f*Vnm - (n+m-1)*g*Vprev)*invs_temp;
                Vprev= temp;
                temp= Wnm;
                Wnm= ((2*n-1)*f*Wnm - (n+m-1)*g*Wprev)*invs_temp;
                Wprev= temp;
            }
            if (m<NMAX && n>=m+2){
                px+= 0.5f*(n-m)*(n-m-1)*(WMM.C(n-1,m+1,dyear)*Vnm+WMM.S(n-1,m+1,dyear)*Wnm);
                py+= 0.5f*(n-m)*(n-m-1)*(-WMM.C(n-1,m+1,dyear)*Wnm+WMM.S(n-1,m+1,dyear)*Vnm);
            }
            if (n>=2 && m>=2){
                px+= 0.5f*(-WMM.C(n-1,m-1,dyear)*Vnm-WMM.S(n-1,m-1,dyear)*Wnm);
                py+= 0.5f*(-WMM.C(n-1,m-1,dyear)*Wnm+WMM.S(n-1,m-1,dyear)*Vnm);
            }
            if (m==1 && n>=2){
                px+= -WMM.C(n-1,0,dyear)*Vnm;
                py+= -WMM.C(n-1,0,dyear)*Wnm;
            }
            if (n>=2 && n>m){
                pz+= (n-m)*(-WMM.C(n-1,m,dyear)*Vnm-WMM.S(n-1,m,dyear)*Wnm);
            }
        }
    }
    return {-px*1.0E-9f,-py*1.0E-9f,-pz*1E-9f};
}
// Model parameters
constexpr
#ifdef PROGMEM
    PROGMEM
#endif /* PROGMEM */
ConstModel WMM2015 = {2015.000000,
{0.0,-29438.5,-2445.3,1351.1,907.2,-232.6,69.5,81.6,24.0,5.4,-1.9,3.1,-2.0,-1501.1,1739.2676859337475,-960.322453658145,257.31453320790104,92.97742019795271,14.707885801905887,-14.381548198001093,1.4333333333333331,1.3118265467998769,-0.8764598212022148,-0.1846372364689991,-0.03396831102433787,483.99273066166324,158.2242796370603,8.966632589774157,9.38815870176489,2.511838636006169,-0.1748771299570375,-0.33665605829585415,0.04926223377768894,0.00259499648053841,-0.024830427232533002,0.00364965934300906,30.66882284086633,-6.673359735450364,-1.4043936159679125,-0.7464209133553132,0.1887584750541334,-0.007846531817819996,-0.00537495036163976,0.0007633810206905742,0.0020197163363339116,0.0009684786719132941,0.49511953422845595,-0.36952022137296814,-0.03044713810352108,0.008224396186997112,-0.0065210774307459494,0.00011779224879003657,-7.711312805134473e-05,-7.901744265512675e-05,-5.587376953345928e-05,0.00319228705617618,0.002954684201426394,0.0008498542726563683,0.0005838511040628811,-0.0003120815423275714,2.303057264678893e-05,4.977631011946968e-06,4.7911362107834415e-06,-0.0045813423970313604,-5.018025581454949e-05,7.925226469630308e-05,-3.029289764645135e-07,-1.0602514176257971e-06,-5.750020635711086e-07,4.7425370883909984e-08,3.2091175418862035e-05,-1.978723788377687e-05,3.803990742829962e-06,3.8572311040429634e-07,1.731729695411081e-08,2.2208964739434834e-08,-6.183511838680272e-07,-6.823734684648174e-07,5.748933983403223e-08,1.6884656654872546e-08,-1.7767171791547867e-09,-1.8558111802048348e-07,-7.298610579975181e-09,-2.564470354147122e-10,-1.9385573719060062e-10,-3.2640378796247345e-09,7.914127330467462e-11,1.193099586284436e-11,1.4763854141620308e-10,-7.916082160021906e-12,0.0},
{0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,4796.2,-1642.907926005999,-47.07102789048341,89.61894888917186,12.238627374015437,-4.5171103278850415,-10.223938994899596,1.6999999999999997,-3.2199378875996976,0.44497190922573976,-0.012309149097933274,-0.11322770341445958,-185.32943640986986,31.629363994027234,-14.057414018548679,9.60773621817831,1.1455088285083082,-0.49891416487743045,-0.3605606304825421,0.1716232660642066,-0.003892494720807615,0.022671259647095352,0.0045620741787613245,-28.370901074477306,3.603614257143197,-1.1892524662877217,0.33813212407775356,0.02036700308869262,0.03236694374850748,0.020286102977801673,0.005852587825294402,-0.0006732387787779705,0.0013409704688030226,-2.320652724442052,0.03779717639202533,-0.06981843737531558,0.013378351130848636,-0.0046217344897519835,-0.0013349788196204146,0.0005654962723765281,-9.657687435626604e-05,-0.00013658032552623381,0.07431347309842688,0.0016340298992736878,0.00030156119352322744,0.0007111569838961407,-0.0001619069655684393,-0.00010702442582919562,5.807236180604796e-06,1.5970454035944803e-06,0.004038559940965585,-0.0004928417981786111,3.861007767255791e-05,2.3628460164232055e-05,-9.087869293935405e-07,-1.6428630387745962e-07,3.3197759618736983e-07,-1.1016373651251144e-05,-1.1253991546398094e-05,4.3724031526781175e-07,-7.530784536464832e-07,-1.818316180181635e-07,-4.441792947886967e-09,6.8018630225483e-07,-2.9244577219920745e-07,-6.998702240664794e-08,-1.4898226460181658e-08,1.33253788436609e-09,1.502323336356295e-07,-4.460262021095944e-09,-3.2055879426839023e-09,9.692786859530031e-11,-7.888091542426441e-09,-3.957063665233731e-10,-5.3689481382799615e-11,-9.701961293064772e-11,-1.759129368893757e-12,1.2567827257223882e-12},
{0.0,10.7,-8.6,3.1,-0.4,-0.2,-0.5,0.2,0.0,0.0,0.0,0.0,0.1,17.9,-1.9052558883257649,-2.5311394008759507,0.2529822128134704,0.025819888974716113,-0.04364357804719848,-0.03779644730092272,0.016666666666666666,-0.0149071198499986,0.0,0.0,0.0,0.6928203230275508,-0.051639777949432225,-0.6857275130999355,-0.06831300510639733,-0.020701966780270625,-0.010286889997472794,-0.009960238411119947,-0.0015891043154093204,-0.001297498240269205,-0.0010795837927188264,0.0,-0.5481281277625191,0.07968190728895957,0.0,0.013801311186847085,0.004728054288446502,0.0012260205965343744,0.0006935419821470658,0.0003816905103452871,9.617696839685295e-05,7.44983593779457e-05,-0.029580398915498084,0.0030519459198529767,-0.0011548914453059721,0.00010965861582662818,-6.331143136646553e-05,-9.816020732503048e-05,-1.2852188008557456e-05,0.0,-6.208196614828809e-06,0.0028210908868533686,6.715191366878169e-05,-3.655287194220939e-05,1.755943170113928e-05,-4.692955523722878e-06,-1.3547395674581724e-06,0.0,0.0,9.692543858317405e-05,-1.612936794039091e-05,1.3547395674581724e-06,3.029289764645135e-07,-1.5146448823225676e-07,0.0,4.7425370883909984e-08,1.4369183023371058e-06,-4.946809470944218e-07,0.0,0.0,0.0,0.0,9.275267758020409e-08,-1.4997219087138844e-08,-4.999073029046282e-09,0.0,0.0,-1.7674392192427e-09,-4.054783655541767e-10,0.0,0.0,-1.813354377569297e-10,-1.9785318326168656e-11,0.0,-4.218244040462945e-12,0.0,0.0},
{0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,-26.8,-15.646192295038858,3.4292856398964493,-0.18973665961010275,0.10327955589886445,0.0,0.1322875655532295,-0.049999999999999996,-0.0298142396999972,0.013483997249264842,0.0,0.0,-3.8393792901110113,-0.051639777949432225,0.39503867602496284,0.07807200583588267,-0.07590721152765897,0.012858612496840992,0.005976143046671968,-0.0015891043154093204,-0.001297498240269205,0.0010795837927188264,0.0,0.12122064363978786,0.05976143046671968,-0.010956262252231943,-0.0040253824294970665,-0.000727392967453308,0.0007356123579206246,-0.0003467709910735329,0.0,0.0,-7.44983593779457e-05,-0.03732764625050948,0.007747247335011402,0.00010499013139145201,-5.482930791331409e-05,0.00018993429409939658,1.96320414650061e-05,0.0,8.77971585056964e-06,0.0,7.423923386456234e-05,0.00022383971222927231,-6.396752589886643e-05,-4.38985792528482e-06,2.346477761861439e-06,-2.7094791349163448e-06,0.0,0.0,8.400204677208417e-05,1.7921519933767679e-06,-1.3547395674581724e-06,0.0,1.5146448823225676e-07,0.0,0.0,4.789727674457019e-07,3.7101071032081634e-07,-8.744806305356236e-08,-1.836776716210935e-08,8.658648477055405e-09,0.0,0.0,2.999443817427769e-08,-4.999073029046282e-09,0.0,0.0,5.302317657728099e-09,4.054783655541767e-10,-1.282235177073561e-10,0.0,-9.066771887846485e-11,0.0,0.0,-4.218244040462945e-12,0.0,0.0}};

constexpr
#ifdef PROGMEM
    PROGMEM
#endif /* PROGMEM */
ConstModel WMM2015v2 = {2015.000000,
{0.0,-29438.2,-2444.5,1351.8,907.5,-232.9,69.4,81.7,24.2,5.5,-2.0,3.0,-2.0,-1493.5,1740.5378565259646,-960.0366798548202,257.66238375051955,92.97742019795271,14.773351168976683,-14.343751750700173,1.4833333333333334,1.3118265467998769,-0.8225238322051553,-0.17232808737106584,-0.011322770341445958,484.6855509846908,157.96608074731316,8.780293591649174,9.35400219921169,2.49458699702261,-0.18259229745514208,-0.33665605829585415,0.04767312946227961,0.00259499648053841,-0.024830427232533002,0.0045620741787613245,30.689904691934117,-6.685312021543709,-1.4073816874912486,-0.742395530925816,0.1898495645053134,-0.007601327698513121,-0.0055483358571765265,0.0007633810206905742,0.0020197163363339116,0.0008939803125353484,0.4908937629548134,-0.36905069123145223,-0.029817197315172368,0.008224396186997112,-0.006552733146429182,0.00011779224879003657,-6.426094004278728e-05,-7.023772680455712e-05,-5.587376953345928e-05,0.0057164210075713,0.0030442200863181035,0.0008315778366852636,0.0005838511040628811,-0.00030973506456570993,2.4385312214247102e-05,4.977631011946968e-06,4.7911362107834415e-06,-0.00454257222159809,-5.376455980130303e-05,7.857489491257399e-05,-3.029289764645135e-07,-1.0602514176257971e-06,-5.750020635711086e-07,4.7425370883909984e-08,2.8259393279296416e-05,-2.015824859409769e-05,3.803990742829962e-06,4.040908775664057e-07,8.658648477055405e-09,2.66507576873218e-08,-6.492687430614286e-07,-6.823734684648174e-07,5.998887634855538e-08,1.6884656654872546e-08,-1.7767171791547867e-09,-1.838136788012408e-07,-7.298610579975181e-09,-2.564470354147122e-10,-2.4231967148825077e-10,-3.2640378796247345e-09,7.914127330467462e-11,1.193099586284436e-11,1.4763854141620308e-10,-7.916082160021906e-12,-0.0},
{0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,4796.3,-1641.0604051445923,-46.417830625741225,89.5873261125702,12.109527929141855,-4.386179593743447,-10.261735442200518,1.6833333333333331,-3.2497521272996948,0.44497190922573976,-0.0,-0.11322770341445958,-184.40567597916643,31.823013161337606,-14.057414018548679,9.588218216719339,1.1317075173214608,-0.5014858873767987,-0.36454472584699005,0.17003416174879726,-0.00518999296107682,0.022671259647095352,0.0027372445072567945,-28.323466909574783,3.5996301617787485,-1.1942325854932816,0.3398572879761095,0.02182178902359924,0.03261214786781436,0.020459488473338443,0.005852587825294402,-0.0005770618103811176,0.0013409704688030226,-2.3241742005034207,0.037562411321267405,-0.07044837816366428,0.01343318043876195,-0.004590078774068751,-0.0013349788196204146,0.0005654962723765281,-9.657687435626604e-05,-0.00013658032552623381,0.07468466926774969,0.0018131016690571056,0.00031983762949433216,0.0007111569838961407,-0.0001619069655684393,-0.00010702442582919562,5.807236180604796e-06,1.5970454035944803e-06,0.003999789765532316,-0.0004964261021653646,4.064218702374517e-05,2.393138914069657e-05,-9.087869293935405e-07,-1.6428630387745962e-07,3.3197759618736983e-07,-1.3890210255925356e-05,-1.13776617831717e-05,4.3724031526781175e-07,-7.714462208085927e-07,-1.818316180181635e-07,-4.441792947886967e-09,7.420214206416327e-07,-2.9244577219920745e-07,-7.248655892117107e-08,-1.4898226460181658e-08,1.33253788436609e-09,1.502323336356295e-07,-4.460262021095944e-09,-3.3338114603912585e-09,9.692786859530031e-11,-7.978759261304907e-09,-3.957063665233731e-10,-5.3689481382799615e-11,-9.701961293064772e-11,-1.759129368893757e-12,1.436323115111301e-12},
{0.0,7.0,-11.0,2.4,-0.8,-0.3,-0.8,-0.3,-0.1,-0.1,0.0,-0.0,0.0,9.0,-3.57957166897568,-2.327015255644019,-0.28460498941515416,0.15491933384829665,-0.10910894511799618,-0.03779644730092272,0.03333333333333333,-0.0149071198499986,-0.0,0.0,0.0,0.08660254037844385,0.2581988897471611,-0.4844813951249545,-0.039036002917941334,-0.003450327796711771,-0.007715167498104595,-0.003984095364447979,-0.0,-0.001297498240269205,-0.0,-0.0,-0.5797509043642028,0.10358647947564745,0.0009960238411119947,0.009200874124564723,0.003273268353539886,0.0012260205965343744,0.0006935419821470658,0.0002544603402301914,0.0,0.0,-0.028171808490950554,0.0028171808490950554,-0.0016798421022632321,5.482930791331409e-05,-3.1655715683232764e-05,-7.85281658600244e-05,-1.2852188008557456e-05,-0.0,-6.208196614828809e-06,0.0010393492741038726,0.0,-5.482930791331408e-05,1.755943170113928e-05,0.0,-2.7094791349163448e-06,-8.296051686578281e-07,-0.0,7.754035086653923e-05,-1.612936794039091e-05,2.7094791349163448e-06,9.087869293935405e-07,-0.0,0.0,0.0,3.352809372119913e-06,-1.2367023677360544e-07,0.0,-1.836776716210935e-08,-0.0,-0.0,1.2367023677360544e-07,-0.0,-4.999073029046282e-09,-0.0,0.0,-5.302317657728099e-09,-4.054783655541767e-10,-1.282235177073561e-10,-0.0,-0.0,-0.0,-0.0,-4.218244040462945e-12,-0.0,-1.7954038938891263e-13},
{0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,-30.2,-17.089567968012922,2.6536138880151094,-0.1264911064067352,0.051639777949432225,0.06546536707079771,0.11338934190276816,-0.06666666666666667,-0.044721359549995794,0.0,0.0,-0.0,-4.994079828490263,-0.10327955589886445,0.43230647564995933,0.11222850838908131,-0.05175491695067656,0.012858612496840992,0.011952286093343936,0.0015891043154093204,0.001297498240269205,0.0010795837927188264,0.0,-0.10540925533894598,0.0756978119245116,-0.0,-0.006900655593423542,-0.002909571869813232,-0.0002452041193068749,-0.0006935419821470658,-0.0002544603402301914,0.0,-7.44983593779457e-05,-0.024650332429581735,0.007747247335011402,0.00041996052556580803,-0.00010965861582662818,0.00018993429409939658,5.8896124395018286e-05,1.2852188008557456e-05,8.77971585056964e-06,6.208196614828809e-06,-0.00044543540318737394,4.4767942445854464e-05,-0.00010052039784107583,-8.77971585056964e-06,2.346477761861439e-06,-1.3547395674581724e-06,-0.0,-0.0,8.400204677208417e-05,1.7921519933767679e-06,-3.3868489186454308e-06,-0.0,1.5146448823225676e-07,-0.0,0.0,9.579455348914039e-07,6.183511838680272e-07,-4.372403152678118e-08,-0.0,8.658648477055405e-09,-0.0,3.091755919340136e-08,3.749304771784711e-08,-2.499536514523141e-09,-0.0,0.0,3.5348784384854e-09,8.109567311083534e-10,-1.282235177073561e-10,0.0,-0.0,-0.0,-0.0,-4.218244040462945e-12,0.0,-1.7954038938891263e-13}};

constexpr
#ifdef PROGMEM
    PROGMEM
#endif /* PROGMEM */
ConstModel WMM2020 = {2020.000000,
{0.0,-29404.5,-2500.0,1363.9,903.1,-234.4,65.9,80.6,23.6,5.0,-1.9,3.0,-2.0,-1450.7,1721.658502723464,-972.0391795944579,255.95475381402863,93.7520168671942,14.315093599481099,-14.513835763554324,1.6333333333333333,1.222383827699885,-0.8360078294544202,-0.17232808737106584,-0.011322770341445958,484.0504656885822,159.59273375272028,6.424968655349396,9.163701684986728,2.518739291599593,-0.21345296744756048,-0.34860834438919813,0.04608402514687029,-0.001297498240269205,-0.026989594817970655,0.0045620741787613245,27.706822765841952,-6.163395528801023,-1.4014055444445763,-0.6986913788341337,0.2054885133055595,-0.0009808164772274995,-0.00242739693751473,0.002162912891956627,0.0023082472415244704,0.0009684786719132941,0.3373574066791329,-0.35496478698597694,-0.038006427563705626,0.008663030650303626,-0.006679356009162114,-0.00021595245611506707,-0.0001156696920770171,-7.901744265512675e-05,-7.44983593779457e-05,0.01017077503944504,0.0030218361150951764,0.0005848459510753503,0.0006716482625685774,-0.0003120815423275714,8.128437404749033e-06,2.488815505973484e-06,3.7264392750537873e-06,-0.004180717250887574,-0.0001290349435231273,9.27996603708848e-05,3.332218741109649e-06,-1.3631803940903108e-06,-5.750020635711086e-07,1.4227611265172995e-07,4.6939331209678796e-05,-2.0405589067644898e-05,3.891438805883525e-06,3.489875760800776e-07,-8.658648477055405e-09,2.2208964739434834e-08,-9.275267758020409e-08,-6.973706875519563e-07,3.499351120332397e-08,1.3905011362836212e-08,-8.883585895773934e-10,-2.103252670898813e-07,-9.73148077330024e-09,-7.693411062441365e-10,-2.4231967148825077e-10,-3.536041036260129e-09,3.957063665233731e-11,5.96549793142218e-12,1.307655652543513e-10,-9.675211528915663e-12,-5.386211681667379e-13},
{0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,4652.9,-1727.2010653076843,-33.55800947612954,89.1762300167483,12.316087040939586,-4.167961703507454,-9.713686956337138,1.4,-3.4733589250496735,0.4584559064750046,-0.0,-0.1358732440973515,-212.1184889002685,31.21624577043178,-11.80643892119889,10.168878760123716,0.8625819491779427,-0.43204937989385733,-0.3047832953802704,0.17639057901043456,-0.00259499648053841,0.028069178610689485,0.0045620741787613245,-28.613342361756885,3.980111269083531,-1.2081769192688496,0.30305379147785055,0.00836501912571304,0.031386127271279984,0.016991778562603112,0.004453055954028349,-0.0004808848419842647,0.0009684786719132941,-2.4657375381704476,0.07559435278405066,-0.06761364461609509,0.01288488735962881,-0.0037353744506214664,-0.001001234114715311,0.0006169050244107579,-3.511886340227856e-05,-0.00011174753906691856,0.07357108075978126,0.002014557410063451,-0.00020104079568215166,0.0006540888308674382,-0.0001454816212354092,-0.00011650760280140282,4.977631011946968e-06,5.323484678648268e-07,0.004400414911676101,-0.00048746534219848084,2.4385312214247102e-05,2.3628460164232055e-05,-1.5146448823225676e-07,-1.6428630387745962e-07,3.3197759618736983e-07,-9.100482581468336e-06,-8.533246337378777e-06,1.7489612610712472e-07,-7.714462208085927e-07,-1.4719702410994188e-07,-4.441792947886967e-09,8.65691657415238e-07,-1.1247914315354133e-07,-8.498424149378678e-08,-1.5891441557527103e-08,2.66507576873218e-09,1.7144160426654187e-07,-4.054783655541767e-10,-3.846705531220682e-09,9.692786859530031e-11,-7.978759261304907e-09,-3.957063665233731e-10,-5.3689481382799615e-11,-1.0967434505203656e-10,-0.0,8.977019469445631e-13},
{0.0,6.7,-11.5,2.8,-1.1,-0.3,-0.6,-0.1,-0.1,-0.1,0.0,-0.0,0.0,7.7,-4.099186911246343,-2.5311394008759507,-0.5059644256269408,0.15491933384829665,-0.08728715609439695,-0.05669467095138408,0.016666666666666666,-0.0298142396999972,-0.0,-0.012309149097933274,-0.0,-0.6350852961085883,0.43893811257017384,-0.447213595499958,-0.034156502553198666,0.017251638983558856,-0.0025717224993681985,-0.0019920476822239894,-0.0,-0.0,-0.0,-0.0,-0.6429964575675704,0.10757057484009544,0.0009960238411119947,0.008050764858994133,0.0025458753860865776,0.0012260205965343744,0.0006935419821470658,0.0002544603402301914,0.0,0.0,-0.03873623667505701,0.0028171808490950554,-0.001469861839480328,0.00010965861582662818,-3.1655715683232764e-05,-5.8896124395018286e-05,-1.2852188008557456e-05,-0.0,-0.0,0.0007423923386456233,-0.0,-4.569108992776174e-05,1.755943170113928e-05,-0.0,-2.7094791349163448e-06,-8.296051686578281e-07,-0.0,5.169356724435949e-05,-1.4337215947014143e-05,3.3868489186454308e-06,9.087869293935405e-07,-0.0,0.0,0.0,4.7897276744570195e-06,0.0,-0.0,-1.836776716210935e-08,-0.0,-0.0,1.2367023677360544e-07,-0.0,-4.999073029046282e-09,-9.93215097345444e-10,0.0,-7.0697568769708e-09,-4.054783655541767e-10,-1.282235177073561e-10,-0.0,-0.0,-1.9785318326168656e-11,-0.0,-4.218244040462945e-12,-0.0,-1.7954038938891263e-13},
{0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,-25.1,-17.435978129526696,2.327015255644019,0.0632455532033676,0.025819888974716113,0.02182178902359924,0.0944911182523068,-0.049999999999999996,-0.044721359549995794,-0.0,-0.0,-0.0,-6.899335716816027,-0.12909944487358055,0.5142956348249517,0.12198750911856666,-0.062105900340811884,0.01543033499620919,0.013944333775567924,0.003178208630818641,0.001297498240269205,0.0010795837927188264,0.0,0.05797509043642029,0.07370576424228761,-0.008964214570007952,-0.008050764858994133,-0.0025458753860865776,-0.0004904082386137498,-0.0006935419821470658,-0.0003816905103452871,0.0,-7.44983593779457e-05,-0.03944053188733077,0.0070429521227376385,0.000944911182523068,-0.00010965861582662818,0.00015827857841616382,7.85281658600244e-05,1.2852188008557456e-05,1.755943170113928e-05,6.208196614828809e-06,0.00037119616932281165,2.2383971222927232e-05,-0.00010965861582662816,-1.3169573775854458e-05,2.346477761861439e-06,-2.7094791349163448e-06,-0.0,-0.0,6.461695905544936e-05,3.5843039867535357e-06,-3.3868489186454308e-06,-0.0,1.5146448823225676e-07,0.0,0.0,1.4369183023371058e-06,4.946809470944218e-07,-8.744806305356236e-08,-0.0,8.658648477055405e-09,-0.0,3.091755919340136e-08,3.749304771784711e-08,-2.499536514523141e-09,-0.0,4.441792947886967e-10,3.5348784384854e-09,8.109567311083534e-10,-1.282235177073561e-10,-0.0,-0.0,0.0,-0.0,-0.0,0.0,-1.7954038938891263e-13}};

}
#endif /* GEOMAG_HPP */