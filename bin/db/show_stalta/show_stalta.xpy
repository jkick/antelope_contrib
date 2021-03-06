import sys
import os

sys.path.append( os.environ['ANTELOPE'] + '/data/python' )

import antelope.datascope as ds
import antelope.stock as stock

from antelope.buplot import *
from antelope.buvector import * 

from math import sqrt

def usage():
	print "usage: show_stalta db sta chan tstart tend filter sta_twin lta_twin SNR_thresh\n"


def dbminmax(db, tmin=-1.e30, tmax=-1.e30, gain=1.0):
			#This will return the minimum and maximum amplitudes
			#of a gain adjusted trace
			#
			#args:
			#	db = Database pointer to a record in a trace table
			#	tmin = Start of time window in which to find min, max
			#	tmax = End of time window in which to find min, max
			#	gain = Constant gain factor to be applied to trace amplitude
			#
			#returns:
			#	amin = Minimum gain adjusted trace amplitude in time window
			#	amax = Maximum gain adjusted trace amplitude in time window
	start = db[3]
	end = db[3]

	if (db[3] < 0):
		start = 0
		end = ds.dbquery ( db, 'dbRECORD_COUNT' ) - 1 

	for db[3] in range(start, end + 1):
		nsamp, samprate, tstart = ds.dbgetv ( db, 'nsamp', 'samprate', 'time' )
		dt = 1.0 / samprate
		data = ds.trdata( db )

		for i in range(nsamp):
			time = tstart + i * dt
			if ( time < tmin ):
				continue
			if ( time > tmax ):
				break
			if ( data[i] > 1.e20 ):
				continue
			if 'amin' not in locals() :
				amin = data[i]
			elif ( amin > data[i] ):
				amin = data[i]
			if 'amax' not in locals():
				amax = data[i]
			elif ( amax < data[i] ):
				amax = data[i]
		
		if 'amin' in locals():
			mid = 0.5*(amin+amax)
			amin = mid - (mid-amin)*gain
			amax = mid + (amax-mid)*gain

	return ( amin, amax )

def time2samp(time, dt):
			# This will return the closest sample value index
			# to some desired time.
			#
			# args:
			#	time	= Desired time.
			#	dt	= Time sampling increment.
			#
			# returns:
			#	index	= Index of sample value.
	if ( time < 0.0 ):
		return int( (time/dt)-0.5 )
	else:
		return int( (time/dt)+0.5 )

def treval(db, evaltime):
			#This will return the trace amplitude at a desired time
			#
			#args:
			#	db = Database pointer to a record in a trace table
			#	evaltime = Desired time at which to sample trace
			#
			#returns:
			#	value = Trace amplitude at desired time

	start = db[3]
	end = db[3]
	if ( db[3] < 0 ):
		start = 0
		end = ds.dbquery ( db, "dbRECORD_COUNT" ) - 1

	for db[3] in range( start, end+1 ):
		nsamp, samprate, tstart = ds.dbgetv ( db, 'nsamp', 'samprate', 'time' )
		dt = 1.0 / samprate
		tend = tstart + dt * (nsamp - 1)
		if ( (evaltime < tstart) or (evaltime > tend) ):
			continue

		i = time2samp ( evaltime-tstart, dt )

		data = ds.trdata ( db )

		value = data[i]
	
	return ( value )

def trstalta2sn2( dbsta, dblta, stawin, ltawin, SNR_thresh ):
			#This will return a database pointer to a record in a trace table with 
			#data representing the signal to noise ratio and a database pointer
			#to a record in a trace table with data representing the long term average
			#with lta_hold values where appropriate
			#
			#args:
			#	dbsta = Database pointer to a record in a trace table with data
			#representing the short term average
			#	dblta = Database pointer to a record in a trace table with data
			#representing the long term average
			#	stawin = The length of the time window used to calculate the
			#short term average
			#	ltawin = The length of the time window used to calculate the
			#long term average
			#	SNR_thresh = The signal to noise ratio threshold
			#
			#returns:
			#	dbratio = A database pointer to a record in a trace table with data
			#representing the signal to noise ratio
			#	dblta = A database pointer to a record in a trace table with data 
			#representing the long term average with lta_hold values where appropriate

	start = dbsta[3]
	end = dbsta[3]
	if ( dbsta[3] < 0 ):
		start = 0
		end = ds.dbquery( dbsta, "dbRECORD_COUNT" ) - 1

	dbratio = ds.dbinvalid()
	dbratio = ds.trcopy( dbsta )

	for dbsta[3] in range( start, end + 1 ):
		dblta[3] = dbsta[3]

		sta = list( ds.trdata( dbsta ) )
		lta = list( ds.trdata( dblta ) )

		noisesq = [ (( lta[i]*lta[i]*ltawin - sta[i]*sta[i]*stawin ) / (ltawin - stawin)) for i in range( len( sta ) ) ]

		for i in range( len( noisesq ) ):
			if noisesq[i] <= 0.0:
				noisesq[i] = 1.e-20

		ratio = [ sta[i] / sqrt( noisesq[i] ) for i in range( len( sta ) ) ]
		ratio = [ ratio[i] if not lta[i] > 1.e20 else 1.e30 for i in range( len( ratio ) ) ]

		#find all indices where SNR >= SNR_tresh
		indices_begin = [ i for (i,SNR) in enumerate( ratio ) if SNR >= SNR_thresh ]

		while( len( indices_begin ) > 0 ):
			index_begin = indices_begin[ 0 ]
			lta_hold = lta[ index_begin ]

			noisesq = [ ( ( lta_hold*lta_hold*ltawin - sta[i]*sta[i]*stawin ) / (ltawin - stawin)) for i in range( len( sta ) ) ]

			for i in range( len( noisesq ) ):
				if noisesq[i] <= 0.0:
					noisesq[i] = 1.e-20

			ratio_temp = [ sta[i] / sqrt( noisesq[i] ) for i in range( len( sta ) ) ]

			#get index, greater than index begin,where SNR <= SNR_thresh
			indices_end = [ i for (i,SNR) in enumerate( ratio_temp ) if ( (SNR < SNR_thresh) and ( i > index_begin ) ) ]

			if( len( indices_end ) == 0 ):
				ratio[ index_begin : ] = ratio_temp[ index_begin : ]
				lta[ index_begin : ] = [ lta_hold ]*len( lta[ index_begin : ] )
				break

			else:
				index_end = indices_end[ 0 ]
				ratio[ index_begin : index_end ] = ratio_temp[ index_begin : index_end ]
				lta[ index_begin : index_end ] = [ lta_hold ]*( index_end - index_begin )

			indices_begin = [ i for ( i, SNR ) in enumerate( ratio ) if ( ( SNR >= SNR_thresh ) and ( i > index_end ) ) ]

		dbratio[3] = dbsta[3]
		ds.trputdata( dbratio, ratio )
		ds.trputdata( dblta, lta )
	
	return dbratio, dblta
	
def tr2buvec( db ):
			#This will return a pointer to a single buvector object containing the
			#data points of all trace segments contained in an input trace table
			#
			#args:
			#	db = Database pointer to a trace table
			#
			#returns:
			#	tr_buvec = A pointer to a buvector object containing the
			#the data points of all trace segments contained in trace table
			#represented by Database pointer 'db'
	start = db[3]
	end = db[3]
	if ( db[3] < 0 ):
		start = 0
		end = ds.dbquery( db, "dbRECORD_COUNT" ) - 1

	tr_buvec = buvector_create( 0, 1, "" )

	for db[3] in range(start, end + 1):
		nsamp, samprate, tstart = ds.dbgetv( db, 'nsamp', 'samprate', 'time' )
		dt = 1.0 / samprate
		tr_data = ds.trdata ( db )

		for i in range( nsamp ):
			buvector_append( tr_buvec, -1, tstart + i*dt, tr_data[i] )

	return tr_buvec


	

if ( len(sys.argv) != 10 ):
	usage()
	exit()


dbname = sys.argv[1]
sta = sys.argv[2]
chan = sys.argv[3]
tstart = sys.argv[4]
tend = sys.argv[5]
filter = sys.argv[6]
sta_twin = sys.argv[7]
lta_twin = sys.argv[8]
SNR_thresh = float(sys.argv[9])

ts = stock.str2epoch( tstart )
te = stock.str2epoch( tend )

db = ds.dbopen( dbname, "r" )
dbwf = db.lookup("", "wfdisc", "", "" )

data = ds.trloadchan( dbwf, ts, te, sta, chan )
data = data.lookup("", "trace", "", "" )
data[3] = 0

ndata = ds.dbquery( data, "dbRECORD_COUNT" )

if ( ndata < 1 ):
	print 'No data for ' + sta + ' ' + chan + ' to process\n'
	exit()

width = 1500
height = 250

mw = Buplot().mw
region = (0, 0, width, height)

canvas_data = BCanvas ( mw,
		width = width,
		height = height,
		background = 'white',
		scrollregion = region,
	)
canvas_data.grid (
			row = 0,
			column = 0,
			sticky = 'nsew',
			padx = 0,
			pady = 0,
		) 

canvas_dataf = BCanvas ( mw,
		width = width,
		height = height,
		background = 'white',
		scrollregion = region,
	)
canvas_dataf.grid (
			row = 1,
			column = 0,
			sticky = 'nsew',
			padx = 0,
			pady = 0,
		)
canvas_stalta = BCanvas ( mw,
		width = width,
		height = height,
		background = 'white',
		scrollregion = region,
	)
canvas_stalta.grid (
			row = 2,
			column = 0,
			sticky = 'nsew',
			padx = 0,
			pady = 0,
		)

region = (0, 0, width, height+45)

canvas_ratio = BCanvas (mw,
		width = width,
		height = height+45,
		background = 'white',
		scrollregion = region,
	)
canvas_ratio.grid (
			row = 3,
			column = 0,
			sticky = 'nsew',
			padx = 0,
			pady = 0,
		)
mw.columnconfigure ( 0, weight = 1 )
mw.rowconfigure ( 0, weight = 1 )
mw.rowconfigure ( 1, weight = 1 )
mw.rowconfigure ( 2, weight = 1 )
mw.rowconfigure ( 3, weight = 1 )

dataf = ds.dbinvalid ()
dataf = ds.trcopy ( data )
ds.trfilter ( dataf, filter)

amin, amax = dbminmax ( data, ts, te, 1.1 )

canvas_data.create_bpviewport ( 'viewdata', 0, 0, \
		mleft = 80, \
		mright = 5, \
		mbottom = 5, \
		mtop = 5, \
		height = 0, \
		width = 0, \
		fill = "#000080", \
		fill_frame = "#f0f0ff", \
		xmode = 'time', \
		xleft = ts, \
		xright = te, \
		ybottom = amin, \
		ytop = amax, \
		tag = 'viewdata'
	)
canvas_data.create_bpaxes ( 'viewdata', 
 		ylabel = sta + ' ' + chan ,
 		xformat = 'time',
 		yformat = 'auto',
                xfill_time_axes = "#4040ff"
 	)
canvas_data.create_bpgrid ( 'viewdata',
                linewidth = 2,
                fill = "#4040ff",
		visible_ysmall = 0,
		visible_xsmall = 0,
		visible_x = 0
        )
data_buvec = tr2buvec( data )
canvas_data.create_bppolyline ( 'viewdata',\
		vector = data_buvec,\
		outline = "yellow"
 	)

(amin, amax) = dbminmax( dataf, ts, te, 1.1 )

canvas_dataf.create_bpviewport ( 'viewdataf', 0, 0, \
		mleft = 80, \
		mright = 5, \
		mbottom = 5, \
		mtop = 5, \
		height = 0, \
		width = 0, \
		fill = "#000080", \
		fill_frame = "#f0f0ff", \
		xmode = 'time', \
		xleft = ts, \
		xright = te, \
		ybottom = amin, \
		ytop = amax, \
		tag = 'viewdataf'
	)
canvas_dataf.create_bpaxes ( 'viewdataf', 
 		ylabel = 'FILTERED',
 		xformat = 'time',
 		yformat = 'auto',
                xfill_time_axes = "#4040ff"
 	)
canvas_dataf.create_bpgrid ( 'viewdataf',
                linewidth = 2,
                fill = "#4040ff",
		visible_ysmall = 0,
		visible_xsmall = 0,
		visible_x = 0,
        )

dataf_buvec = tr2buvec( dataf )
canvas_dataf.create_bppolyline ( 'viewdataf',
 		vector = dataf_buvec,
		outline = 'yellow'
 	)

datasta = ds.dbinvalid ()
datasta = ds.trcopy ( dataf )

ds.trfilter ( datasta, "SQ ; AVE " + sta_twin + " ; SQRT" )

astamin, astamax = dbminmax ( datasta, ts, te, 1.1 )

datalta = ds.trcopy ( dataf )

ds.trfilter ( datalta, "SQ ; AVE " + lta_twin + " ; SQRT" )

altamin, altamax = dbminmax ( datalta, ts, te, 1.1 )

aslmax = astamax
if (altamax > aslmax):
	aslmax = altamax
aslmin = 0.0

canvas_stalta.create_bpviewport ( 'viewstalta', 0, 0, \
		mleft = 80, \
		mright = 5, \
		mbottom = 5, \
		mtop = 5, \
		height = 0, \
		width = 0, \
		fill = "#000080", \
		fill_frame = "#f0f0ff", \
		xmode = 'time', \
		xleft = ts, \
		xright = te, \
		ybottom = aslmin, \
		ytop = aslmax, \
		tag = 'viewstalta'
	)
canvas_stalta.create_bpaxes ( 'viewstalta', 
 		ylabel = "STA LTA" ,
 		xformat = 'time',
 		yformat = 'auto',
                xfill_time_axes = "#4040ff"
 	)
canvas_stalta.create_bpgrid ( 'viewstalta',
                linewidth = 2,
                fill = "#4040ff",
		visible_ysmall = 0,
		visible_xsmall = 0,
		visible_x = 0
        )

datasta_buvec = tr2buvec( datasta )
canvas_stalta.create_bppolyline ( 'viewstalta',
 		vector = datasta_buvec,
		outline = 'yellow'
 	)

dataratio, datalta = trstalta2sn2( datasta, datalta, float(sta_twin), float(lta_twin), SNR_thresh )

datalta_buvec = tr2buvec( datalta )
canvas_stalta.create_bppolyline ( 'viewstalta',
 		vector = datalta_buvec,
		outline = 'orange'
 	)

armin, armax = dbminmax( dataratio, ts, te, 1.1 )

canvas_ratio.create_bpviewport ( 'viewratio', 0, 0, \
		mleft = 80, \
		mright = 5, \
		mbottom = 50, \
		mtop = 5, \
		height = 0, \
		width = 0, \
		fill = "#000080", \
		fill_frame = "#f0f0ff", \
		xmode = 'time', \
		xleft = ts, \
		xright = te, \
		ybottom = armin, \
		ytop = armax, \
		tag = 'viewratio'
	)
canvas_ratio.create_bpaxes ( 'viewratio', 
 		ylabel = 'S/N' ,
 		xformat = 'time',
 		yformat = 'auto',
                xfill_time_axes = "#4040ff"
 	)
canvas_ratio.create_bpgrid ( 'viewratio',
                linewidth = 2,
                fill = "#4040ff",
		visible_ysmall = 0,
		visible_xsmall = 0,
		visible_x = 0
        )

dataratio_buvec = tr2buvec( dataratio )
canvas_ratio.create_bppolyline ( 'viewratio',
 		vector = dataratio_buvec,
		outline = 'yellow'
 	)

def bindpanleft(e):
	x = e.x
	y = e.y
	w = e.widget
	id = w.winfo_id()

	( ret, vp, inside, entries ) = buplot_viewport_locate ( str(id), x, y, 1 )

	if not inside:
		return

	t1 = buplot_viewport_pixels2wcoords ( vp, x, y )
	t0 = float ( w.itemcget ( vp, 'xleft' ) )

	tshift = t1[1] - t0
	t1 = float ( w.itemcget ( vp, 'xright' ) )

	t0 += tshift
	t1 += tshift

	amin, amax = dbminmax ( data, t0, t1, 1.1 )
	canvas_data.itemconfigure ( 'viewdata', xleft=t0, xright=t1,ybottom=amin, ytop=amax )
	amin, amax = dbminmax ( dataf, t0, t1, 1.1 )
	canvas_dataf.itemconfigure ( 'viewdataf', xleft=t0, xright=t1, ybottom=amin, ytop=amax )

	astamin, astamax = dbminmax ( datasta, t0, t1, 1.1 )
	altamin, altamax = dbminmax ( datalta, t0, t1, 1.1 )
	aslmax = astamax
	if (altamax > aslmax):
		aslmax = altamax
	aslmin = 0.0

	canvas_stalta.itemconfigure ( 'viewstalta', xleft=t0, xright=t1, ybottom=aslmin, ytop=aslmax )

	armin, armax = dbminmax ( dataratio, t0, t1, 1.1 )
	canvas_ratio.itemconfigure ( 'viewratio', xleft=t0, xright=t1, ybottom=armin, ytop=armax )


def bindpanright(e):
	x = e.x
	y = e.y
	w = e.widget
	id = w.winfo_id()

	( ret, vp, inside, entries ) = buplot_viewport_locate ( str(id), x, y, 1 )

	if not inside:
		return

	t1 = buplot_viewport_pixels2wcoords ( vp, x, y )
	t0 = float ( w.itemcget ( vp, 'xleft' ) )
	tshift = t0 - t1[1]
	t1 = float ( w.itemcget ( vp, 'xright' ) )
	t0 += tshift
	t1 += tshift

	amin, amax = dbminmax ( data, t0, t1, 1.1 )
	canvas_data.itemconfigure ( 'viewdata', xleft=t0, xright=t1, ybottom=amin, ytop=amax )
	amin, amax = dbminmax ( dataf, t0, t1, 1.1 )
	canvas_dataf.itemconfigure ( 'viewdataf', xleft=t0, xright=t1, ybottom=amin, ytop=amax )

	astamin, astamax = dbminmax ( datasta, t0, t1, 1.1 )
	altamin, altamax = dbminmax ( datalta, t0, t1, 1.1 )
	aslmax = astamax
	if (altamax > aslmax):
		aslmax = altamax
	aslmin = 0.0

	canvas_stalta.itemconfigure ( 'viewstalta', xleft=t0, xright=t1, ybottom=aslmin, ytop=aslmax )

	armin, armax = dbminmax ( dataratio, t0, t1, 1.1 )
	canvas_ratio.itemconfigure ( 'viewratio', xleft=t0, xright=t1, ybottom=armin, ytop=armax )


def bindzoom(e):
	x = e.x
	y = e.y
	w = e.widget
	id = w.winfo_id()
	( ret, vp, inside, entries ) = buplot_viewport_locate ( str(id), x, y, 1 )

	if not inside:
		return

	t0 = float ( w.itemcget ( vp, 'xleft' ) )
	t1 = float ( w.itemcget ( vp, 'xright' ) )
	twin = t1 - t0

	key = e.char
	if (key == "i"):
		twin = twin/1.2
	elif(key == "I"):
		twin = twin/2.0
	elif(key == "o"):
		twin = twin*1.2
	elif(key == "O"):
		twin = twin*2.0
	else:
		return

	t1 = t0 + twin

	amin, amax = dbminmax ( data, t0, t1, 1.1 )
	canvas_data.itemconfigure ( 'viewdata', xleft=t0, xright=t1, ybottom=amin, ytop=amax )
	amin, amax = dbminmax ( dataf, t0, t1, 1.1 )
	canvas_dataf.itemconfigure ( 'viewdataf', xleft=t0, xright=t1, ybottom=amin, ytop=amax )

	astamin, astamax = dbminmax ( datasta, t0, t1, 1.1 )
	altamin, altamax = dbminmax ( datalta, t0, t1, 1.1 )
	aslmax = astamax
	if (altamax > aslmax):
		aslmax = altamax
	aslmin = 0.0

	canvas_stalta.itemconfigure ( 'viewstalta', xleft=t0, xright=t1, ybottom=aslmin, ytop=aslmax )

	armin, armax = dbminmax ( dataratio, t0, t1, 1.1 )
	canvas_ratio.itemconfigure ( 'viewratio', xleft=t0, xright=t1, ybottom=armin, ytop=armax )

def bindenter(e):
	e.widget.focus_set ()


def bindstartdrag(e):
	x = e.x
	y = e.y
	w = e.widget
	id = w.winfo_id()

	( ret, vp, inside, entries ) = buplot_viewport_locate ( str(id), x, y, 1 )
	if not inside:
		return

	global dragwindow
	global xstart

	dragwindow = vp
	xstart = x

def binddrag(e):
	global xstart

	x = e.x

	delx = xstart - x

	canvas_data.itemconfigure ( 'viewdata', xtranslate=delx )
	canvas_dataf.itemconfigure ( 'viewdataf', xtranslate=delx )
	canvas_stalta.itemconfigure ( 'viewstalta', xtranslate=delx )
	canvas_ratio.itemconfigure ( 'viewratio', xtranslate=delx )


def bindstopdrag(w):
	canvas_data.itemconfigure ( 'viewdata', xtranslate='apply' )
	canvas_dataf.itemconfigure ( 'viewdataf', xtranslate='apply' )
	canvas_stalta.itemconfigure ( 'viewstalta', xtranslate='apply' )
	canvas_ratio.itemconfigure ( 'viewratio', xtranslate='apply' )

	t0 = float( canvas_data.itemcget ( 'viewdata', 'xleft' ) )
	t1 = float( canvas_data.itemcget ( 'viewdata', 'xright' ) )

	amin, amax = dbminmax ( data, t0, t1, 1.1 )
	canvas_data.itemconfigure ( 'viewdata', ybottom=amin, ytop=amax )
	amin, amax = dbminmax ( dataf, t0, t1, 1.1 )
	canvas_dataf.itemconfigure ( 'viewdataf', ybottom=amin, ytop=amax )

	astamin, astamax = dbminmax ( datasta, t0, t1, 1.1 )
	altamin, altamax = dbminmax ( datalta, t0, t1, 1.1 )
	aslmax = astamax
	if (altamax > aslmax):
		aslmax = altamax
	aslmin = 0.0

	canvas_stalta.itemconfigure ( 'viewstalta', ybottom=aslmin, ytop=aslmax )

	armin, armax = dbminmax ( dataratio, t0, t1, 1.1 )
	canvas_ratio.itemconfigure ( 'viewratio', xleft=t0, xright=t1, ybottom=armin, ytop=armax )

canvas_data.bind ( '<Any-Enter>', bindenter )
canvas_data.bind ( '<KeyPress>', bindzoom )
canvas_dataf.bind ( '<Any-Enter>', bindenter )
canvas_dataf.bind ( '<KeyPress>', bindzoom )
canvas_stalta.bind ( '<Any-Enter>', bindenter )
canvas_stalta.bind ( '<KeyPress>', bindzoom )
canvas_ratio.bind ( '<Any-Enter>', bindenter )
canvas_ratio.bind ( '<KeyPress>', bindzoom )

canvas_data.bind ( '<ButtonRelease-1>', bindpanleft )
canvas_dataf.bind ( '<ButtonRelease-1>', bindpanleft )
canvas_stalta.bind ( '<ButtonRelease-1>', bindpanleft )
canvas_ratio.bind ( '<ButtonRelease-1>', bindpanleft )

canvas_data.bind ( '<ButtonRelease-3>', bindpanright )
canvas_dataf.bind ( '<ButtonRelease-3>', bindpanright )
canvas_stalta.bind ( '<ButtonRelease-3>', bindpanright )
canvas_ratio.bind ( '<ButtonRelease-3>', bindpanright )

canvas_data.bind ( '<ButtonPress-2>', bindstartdrag )
canvas_dataf.bind ( '<ButtonPress-2>', bindstartdrag )
canvas_stalta.bind ( '<ButtonPress-2>', bindstartdrag )
canvas_ratio.bind ( '<ButtonPress-2>', bindstartdrag )
canvas_data.bind ( '<Button2-Motion>', binddrag )
canvas_dataf.bind ( '<Button2-Motion>', binddrag )
canvas_stalta.bind ( '<Button2-Motion>', binddrag )
canvas_ratio.bind ( '<Button2-Motion>', binddrag )
canvas_data.bind ( '<ButtonRelease-2>', bindstopdrag )
canvas_dataf.bind ( '<ButtonRelease-2>', bindstopdrag )
canvas_stalta.bind ( '<ButtonRelease-2>', bindstopdrag )
canvas_ratio.bind ( '<ButtonRelease-2>', bindstopdrag )

mw.mainloop()
