#ifndef _ARITH_H_
#define _ARITH_H_

#ifndef PI
#define	PI	3.14159265358979323846
#endif

/* Escape interface */
retCode m_band(processpo p,objPo *args);
retCode m_bor(processpo p,objPo *args);
retCode m_bxor(processpo p,objPo *args);
retCode m_bleft(processpo p,objPo *args);
retCode m_bright(processpo p,objPo *args);
retCode m_bnot(processpo p,objPo *args);
retCode m_plus(processpo p,objPo *args);
retCode m_minus(processpo p,objPo *args);
retCode m_times(processpo p,objPo *args);
retCode m_div(processpo p,objPo *args);
retCode m_quo(processpo p,objPo *args);
retCode m_rem(processpo p,objPo *args);
retCode m_abs(processpo p,objPo *args);
retCode m_itrunc(processpo p,objPo *args);
retCode m_number(processpo p,objPo *args);
retCode m_trunc(processpo p,objPo *args);
retCode m_ceil(processpo p,objPo *args);
retCode m_floor(processpo p,objPo *args);
retCode m_sqrt(processpo p,objPo *args);
retCode m_cbrt(processpo p,objPo *args);
retCode m_pow(processpo p,objPo *args);
retCode m_exp(processpo p,objPo *args);
retCode m_log(processpo p,objPo *args);
retCode m_log10(processpo p,objPo *args);
retCode m_pi(processpo p,objPo *args);
retCode m_sin(processpo p,objPo *args);
retCode m_cos(processpo p,objPo *args);
retCode m_tan(processpo p,objPo *args);
retCode m_asin(processpo p,objPo *args);
retCode m_acos(processpo p,objPo *args);
retCode m_atan(processpo p,objPo *args);
retCode m_sinh(processpo p,objPo *args);
retCode m_cosh(processpo p,objPo *args);
retCode m_tanh(processpo p,objPo *args);
retCode m_asinh(processpo p,objPo *args);
retCode m_acosh(processpo p,objPo *args);
retCode m_atanh(processpo p,objPo *args);
retCode m_seed(processpo p,objPo *args);
retCode m_rand(processpo p,objPo *args);
retCode m_irand(processpo p,objPo *args);

objPo str2num(uniChar *buff,long len);


#endif


