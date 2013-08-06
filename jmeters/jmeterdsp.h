
#ifndef __JMETERDSP_H
#define	__JMETERDSP_H

class JmeterDSP
{
public:

    JmeterDSP (void) {};
    virtual ~JmeterDSP (void) {};

    virtual void process (float *p, int n) = 0;
    virtual float read (void) = 0;
};

#endif
