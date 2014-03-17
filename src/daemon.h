//
// Copyright (C) 2014 OnlineCity
//

#pragma once

#if !defined(__APPLE__)
#define HAVE_DAEMON
#endif

#ifndef HAVE_DAEMON
int oc_daemon(int nochdir, int noclose);
#else
#define oc_daemon(a,b)  daemon(a,b)
#endif

