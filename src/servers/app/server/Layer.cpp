#include <View.h>
#include <Message.h>
#include <AppDefs.h>
#include <Region.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "Layer.h"
#include "ServerWindow.h"
#include "WinBorder.h"
#include "RGBColor.h"
#include "RootLayer.h"
#include "DisplayDriver.h"
#include "LayerData.h"
#include <stdio.h>

//#define DEBUG_LAYER
#ifdef DEBUG_LAYER
#	define STRACE(x) printf x
#else
#	define STRACE(x) ;
#endif

//#define DEBUG_LAYER_REBUILD
#ifdef DEBUG_LAYER_REBUILD
#	define RBTRACE(x) printf x
#else
#	define RBTRACE(x) ;
#endif

BRegion		gRedrawReg;
BList		gCopyRegList;
BList		gCopyList;
enum{
	B_LAYER_NONE		= 0x00001000UL,
	B_LAYER_MOVE		= 0x00002000UL,
	B_LAYER_SIMPLE_MOVE	= 0x00004000UL,
	B_LAYER_RESIZE		= 0x00008000UL,
	B_LAYER_MASK_RESIZE	= 0x00010000UL,
};

Layer::Layer(BRect frame, const char *name, int32 token, uint32 resize,
				uint32 flags, DisplayDriver *driver)
{
	// frame is in _parent coordinates
	if(frame.IsValid())
		_frame		= frame;
	else
		// TODO: Decorator class should have a method witch returns the minimum frame width.
		_frame.Set(0.0f, 0.0f, 5.0f, 5.0f);

	_boundsLeftTop.Set( 0.0f, 0.0f );

	_name			= new BString(name);
	_layerdata		= new LayerData();

	// driver init
	if (!driver)
		debugger("You MUST have a valid driver to init a Layer object\n");
	fDriver			= driver;

	// Layer does not start out as a part of the tree
	_parent			= NULL;
	_uppersibling	= NULL;
	_lowersibling	= NULL;
	_topchild		= NULL;
	_bottomchild	= NULL;
	
	fCurrent		= NULL;
	fRootLayer		= NULL;

	_flags			= flags;
	_resize_mode	= resize;
	_hidden			= false;
	_is_updating	= false;
	fIsTopLayer		= false;
	_level			= 0;
	_view_token		= token;
	
	_serverwin		= NULL;
	clipToPicture	= NULL;

	/* NOW all regions (_visible, _fullVisible, _full) are empty */
	STRACE(("Layer(%s) successfuly created\n", GetName()));
}

//! Destructor frees all allocated heap space
Layer::~Layer(void)
{
	if(_layerdata)
	{
		delete _layerdata;
		_layerdata = NULL;
	}

	if(_name)
	{
		delete _name;
		_name = NULL;
	}

// TODO: uncomment!
	//PruneTree();

//	_serverwin->RemoveChild(fDriver);
//	delete fDriver;

	if (clipToPicture)
	{
		delete clipToPicture;
		clipToPicture = NULL;
// TODO: allocate and relase a ServerPicture Object.
	}
}

void Layer::AddChild(Layer *layer, RootLayer *rootLayer)
{
STRACE(("Layer(%s)::AddChild(%s) START\n", GetName(), layer->GetName()));
	if( layer->_parent != NULL ) {
		printf("ERROR: AddChild(): Layer already has a _parent\n");
		return;
	}

		// attach layer to the tree structure
	layer->_parent		= this;
	if( _bottomchild ){
		layer->_uppersibling		= _bottomchild;
		_bottomchild->_lowersibling	= layer;
	}
	else{
		_topchild		= layer;
	}
	_bottomchild		= layer;

	layer->SetRootLayer(fRootLayer);
	// higher level objects like RootLayers do not have ServerWindow objects attached
	// This is in case o a WinBorder object which already has a valid _serverwin memeber.
	if(!(layer->_serverwin))
		layer->SetServerWindow(this->SearchForServerWindow());
	layer->RebuildFullRegion();

	Layer		*c = layer->_topchild; //c = short for: current
	Layer		*stop = layer;

	if( c != NULL )
		while( true ){
			// action block
			{
				c->SetRootLayer(fRootLayer);
				c->SetServerWindow(c->SearchForServerWindow());
				c->RebuildFullRegion();
			}

			// tree parsing algorithm
				// go deep
			if(	c->_topchild ){
				c = c->_topchild;
			}
				// go right or up
			else
					// go right
				if( c->_lowersibling ){
					c = c->_lowersibling;
				}
					// go up
				else{
					while( !c->_parent->_lowersibling && c->_parent != stop ){
						c = c->_parent;
					}
						// that enough! We've reached this view.
					if( c->_parent == stop )
						break;
						
					c = c->_parent->_lowersibling;
				}
		}
	
	if ( !(layer->IsHidden()) && layer->fRootLayer && !IsHidden()){
		FullInvalidate( layer->_full );
	}
STRACE(("Layer(%s)::AddChild(%s) END\n", GetName(), layer->GetName()));
}

void Layer::RemoveChild(Layer *layer)
{
STRACE(("Layer(%s)::RemoveChild(%s) START\n", GetName(), layer->GetName()));
	if( layer->_parent == NULL ){
		printf("ERROR: RemoveChild(): Layer doesn't have a _parent\n");
		return;
	}
	if( layer->_parent != this ){
		printf("ERROR: RemoveChild(): Layer is not a child of this layer\n");
		return;
	}

	// Take care of _parent
	layer->_parent		= NULL;
	if( _topchild == layer )
		_topchild		= layer->_lowersibling;
	if( _bottomchild == layer )
		_bottomchild	= layer->_uppersibling;

	// Take care of siblings
	if( layer->_uppersibling != NULL )
		layer->_uppersibling->_lowersibling	= layer->_lowersibling;
	if( layer->_lowersibling != NULL )
		layer->_lowersibling->_uppersibling = layer->_uppersibling;
	layer->_uppersibling	= NULL;
	layer->_lowersibling	= NULL;

	Layer		*c = layer->_topchild; //c = short for: current
	Layer		*stop = layer;

	if( c != NULL )
		while( true ){
			// action block
			{
				c->SetRootLayer(NULL);
				c->SetServerWindow(NULL);
				c->_full.MakeEmpty();
				c->_fullVisible.MakeEmpty();
				c->_visible.MakeEmpty();
			}

			// tree parsing algorithm
				// go deep
			if(	c->_topchild ){
				c = c->_topchild;
			}
				// go right or up
			else
					// go right
				if( c->_lowersibling ){
					c = c->_lowersibling;
				}
					// go up
				else{
					while( !c->_parent->_lowersibling && c->_parent != stop ){
						c = c->_parent;
					}
						// that enough! We've reached this view.
					if( c->_parent == stop )
						break;
						
					c = c->_parent->_lowersibling;
				}
		}

	if ( !(layer->IsHidden()) && layer->fRootLayer){
		PrintTree();
		FullInvalidate( layer->_fullVisible );
	}

	layer->fRootLayer	= NULL;
	layer->_serverwin	= NULL;
	layer->_full.MakeEmpty();
	layer->_fullVisible.MakeEmpty();
	layer->_visible.MakeEmpty();
STRACE(("Layer(%s)::RemoveChild(%s) END\n", GetName(), layer->GetName()));
}

void Layer::RemoveSelf()
{
	// A Layer removes itself from the tree (duh)
	if( _parent == NULL ){
		printf("ERROR: RemoveSelf(): Layer doesn't have a _parent\n");
		return;
	}
	_parent->RemoveChild(this);
}

bool Layer::HasChild(Layer* layer){
	for(Layer *lay = VirtualTopChild(); lay; lay = VirtualLowerSibling()){
		if(lay == layer)
			return true;
	}
	return false;
}

Layer* Layer::LayerAt(const BPoint &pt)
{
	if (_visible.Contains(pt)){
		return this;
	}

	if (_fullVisible.Contains(pt)){
		Layer		*lay = NULL;
		for ( Layer* child = VirtualBottomChild(); child; child = VirtualUpperSibling() ){
			if ( (lay = child->LayerAt( pt )) )
				return lay;
		}
	}

	return NULL;
}

BRect Layer::Bounds(void) const
{
	BRect		r(_frame);
	r.OffsetTo( _boundsLeftTop );
	return r;
}

BRect Layer::Frame(void) const
{
	return _frame;
}

void Layer::PruneTree(void)
{
	Layer		*lay,
				*nextlay;

	lay			= _topchild;
	_topchild	= NULL;
	
	while(lay != NULL)
	{
		if(lay->_topchild != NULL) {
			lay->PruneTree();
		}
		nextlay				= lay->_lowersibling;
		lay->_lowersibling	= NULL;

		delete lay;
		lay					= nextlay;
	}
	// Man, this thing is short. Elegant, ain't it? :P
}

Layer* Layer::FindLayer(const int32 token)
{
	// recursive search for a layer based on its view token
	Layer		*lay,
				*trylay;

	// Search child layers first
	for(lay = VirtualTopChild(); lay; lay = VirtualLowerSibling())
	{
		if(lay->_view_token == token)
			return lay;
	}
	
	// Hmmm... not in this layer's children. Try lower descendants
	for(lay = VirtualTopChild(); lay != NULL; lay = VirtualLowerSibling())
	{
		trylay		= lay->FindLayer(token);
		if(trylay)
			return trylay;
	}
	
	// Well, we got this far in the function, so apparently there is no match to be found
	return NULL;
}

void Layer::SetServerWindow(ServerWindow *win){
	_serverwin		= win;
}

ServerWindow* Layer::SearchForServerWindow() const {
	if (_serverwin)
		return _serverwin;

	if(_parent)
		return _parent->SearchForServerWindow();

	return NULL;
}

void Layer::FullInvalidate(const BRect &rect)
{
	FullInvalidate( BRegion(rect) );
}

void Layer::FullInvalidate(const BRegion& region)
{
STRACE(("Layer(%s)::FullInvalidate():\n", GetName()));
#ifdef DEBUG_LAYER
 region.PrintToStream();
 printf("\n");
#endif

	BPoint		pt(0,0);
	StartRebuildRegions(region, NULL,/* B_LAYER_INVALIDATE, pt); */B_LAYER_NONE, pt);

	Redraw(gRedrawReg);

	EmptyGlobals();
}

void Layer::Invalidate(const BRegion& region)
{
STRACE(("Layer(%s)::Invalidate():\n", GetName()));
#ifdef DEBUG_LAYER
 region.PrintToStream();
 printf("\n");
#endif

	gRedrawReg	= region;

	Redraw(gRedrawReg);

	EmptyGlobals();
}

void Layer::Redraw(const BRegion& reg, Layer *startFrom)
{
STRACE(("Layer(%s)::Redraw();\n", GetName()));
	BRegion		*pReg = const_cast<BRegion*>(&reg);

	if(_serverwin){
		if (pReg->CountRects() > 0){
			RequestClientUpdate(reg, startFrom);
		}
	}
		// call Draw() for all server layers
	else{
		if (pReg->CountRects() > 0){
			RequestDraw(reg, startFrom);
		}
	}
STRACE(("Layer::Redraw ENDED\n"));
}

void Layer::RequestClientUpdate(const BRegion &reg, Layer *startFrom){
STRACE(("Layer(%s)::RequestClientUpdate()\n", GetName()));
	if (IsHidden()){
		// this layer has nothing visible on screen, so bail out.
		return;
	}
// TODO: remove!
	if (_visible.CountRects() > 0){
		fUpdateReg		= _visible;
		fUpdateReg.IntersectWith(&reg);

			// NOTE: CLEAR to the background color!

			// draw itself.
		if (fUpdateReg.CountRects() > 0){
			fDriver->ConstrainClippingRegion(&fUpdateReg);
			Draw(fUpdateReg.Frame());
			fDriver->ConstrainClippingRegion(NULL);
		}

		fUpdateReg.MakeEmpty();
	}
	return;


// TODO: use startFrom!

// TODO: Do that! here? or after a message sent by the client just before calling BView::Draw(r)
		// clear the area in the low color
		// only the visible area is cleared, because DisplayDriver does the clipping to it.
		// draw background, *only IF* our view color is different to B_TRANSPARENT_COLOR!

	BMessage		msg;

	msg.what		= _UPDATE_;
	msg.AddInt32("_token", _view_token);
	msg.AddRect("_rect", ConvertFromTop(reg.Frame()) );
		// for test purposes only!
	msg.AddRect("_rect2", reg.Frame());

	_serverwin->SendMessageToClient( &msg );
}

void Layer::RequestDraw(const BRegion &reg, Layer *startFrom, bool redraw)
{
STRACE(("Layer(%s)::RequestDraw()\n", GetName()));
	if (_visible.CountRects() > 0 && !IsHidden()){
		fUpdateReg		= _visible;
		fUpdateReg.IntersectWith(&reg);

			// NOTE: do not clear background for internal server layers!

			// draw itself.
		if (fUpdateReg.CountRects() > 0){
			fDriver->ConstrainClippingRegion(&fUpdateReg);
			Draw(fUpdateReg.Frame());
			fDriver->ConstrainClippingRegion(NULL);
		}

		fUpdateReg.MakeEmpty();
	}

		// tell children to draw. YES, it's OK - if we're hidden don't tell children to draw
	if (!IsHidden())
	for (Layer *lay = VirtualBottomChild(); lay != NULL; lay = VirtualUpperSibling()){
		if (!redraw && startFrom && lay == startFrom)
			redraw = true;

		if ((startFrom && redraw) || !startFrom){
			if ( !(lay->IsHidden()) ){
				lay->RequestDraw( reg, startFrom, redraw );
			}
		}
	}
}

void Layer::Draw(const BRect &r)
{
// TODO/NOTE: this should be an empty method! the next lines are for testing only
STRACE(("Layer::Draw() Called\n"));
//	RGBColor	col(152,102,51);
//	DRIVER->FillRect_(r, 1, col, &fUpdateReg);
//snooze(1000000);
	fDriver->FillRect(r, _layerdata->viewcolor);

	// empty HOOK function.
}


void Layer::Show(void)
{
STRACE(("Layer(%s)::Show()\n", GetName()));
	if( !IsHidden() )
		return;
	
	_hidden		= false;

	if(_parent)
		_parent->FullInvalidate(_full);
	else
		FullInvalidate( _full );
}

void Layer::Hide(void)
{
STRACE(("Layer(%s)::Hide()\n", GetName()));
	if ( IsHidden() )
		return;

	_hidden			= true;

	Layer		*c = _topchild; //c = short for: current
	Layer		*stop = this;

	if( c != NULL )
		while( true ){
			// action block
			{
				c->_fullVisible.MakeEmpty();
				c->_visible.MakeEmpty();
			}

			// tree parsing algorithm
				// go deep
			if(	c->_topchild ){
				c = c->_topchild;
			}
				// go right or up
			else
					// go right
				if( c->_lowersibling ){
					c = c->_lowersibling;
				}
					// go up
				else{
					while( !c->_parent->_lowersibling && c->_parent != stop ){
						c = c->_parent;
					}
						// that enough! We've reached this view.
					if( c->_parent == stop )
						break;
						
					c = c->_parent->_lowersibling;
				}
		}

	if(_parent)
		_parent->FullInvalidate( _fullVisible );
	else
		FullInvalidate( _fullVisible );

	_fullVisible.MakeEmpty();
	_visible.MakeEmpty();
}

bool Layer::IsHidden(void) const
{
	if (_hidden)
		return true;
	else
		if (_parent)
			return _parent->IsHidden();

	return false;
}

uint32 Layer::CountChildren(void) const
{
	uint32		i = 0;
	Layer		*lay = VirtualTopChild();
	while(lay != NULL)
	{
		lay		= VirtualLowerSibling();
		i++;
	}
	return i;
}

void Layer::RebuildFullRegion( ){
STRACE(("Layer(%s)::RebuildFullRegion()\n", GetName()));
	if (_parent)
		_full.Set( _parent->ConvertToTop( _frame ) );
	else
		_full.Set( _frame );

// TODO: Convert to screen coordinates!
	LayerData		*ld;
	ld			= _layerdata;
	do{
			// clip to user region
		if(ld->clipReg)
			_full.IntersectWith( ld->clipReg );
	} while( (ld = ld->prevState) );

		// clip to user picture region
	if(clipToPicture)
		if(clipToPictureInverse) {
			_full.Exclude( clipToPicture );
		}
		else{
			_full.IntersectWith( clipToPicture );
		}

	if(IsTopLayer() && _serverwin){
		if (_serverwin->fWinBorder->fDecorator){
			// decorator overlaping topLayer? make decorator be in front.
			_full.Exclude( _serverwin->fWinBorder->fDecFull );
		}
	}
}

void Layer::RebuildRegions( const BRegion& reg, uint32 action, BPoint pt, BPoint ptOffset)
{
STRACE(("Layer(%s)::RebuildRegions() START\n", GetName()));
//NOTE: this method must be executed as fast as possible.
	// Currently SendView[Moved/Resized]Msg() simply constructs a message and calls
	//		ServerWindow::SendMessageToClient()
	//   this involves the alternative use of kernel and this code in the CPU, so, a lot
	//   of context switches. This is NOT good at all!
	// One alternative would be the use of a BMessageQueue per ServerWindows OR only
	//   one for app_server which will be emptied as soon as this critical operation ended.
	// Talk to DW, Gabe.

	BRegion		oldRegion;
	uint32		newAction	= action;
	BPoint		newPt		= pt;
	BPoint		newOffset	= ptOffset; // used for resizing only
	
	BPoint		dummyNewLocation;

	RRLabel1:
	switch(action){
		case B_LAYER_NONE:{
RBTRACE(("1) Action B_LAYER_NONE\n"));
			oldRegion		= _visible;
			break;
		}
		case B_LAYER_MOVE:{
RBTRACE(("1) Action B_LAYER_MOVE\n"));
			oldRegion	= _fullVisible;
			_frame.OffsetBy(pt.x, pt.y);
			_full.OffsetBy(pt.x, pt.y);
// TODO: uncomment later when you'll implement a queue in ServerWindow::SendMessgeToClient()
			//SendViewMovedMsg();

			newAction	= B_LAYER_SIMPLE_MOVE;
			break;
		}
		case B_LAYER_SIMPLE_MOVE:{
RBTRACE(("1) Action B_LAYER_SIMPLE_MOVE\n"));
			_full.OffsetBy(pt.x, pt.y);

			break;
		}
		case B_LAYER_RESIZE:{
RBTRACE(("1) Action B_LAYER_RESIZE\n"));
			oldRegion	= _visible;

			_frame.right	+= pt.x;
			_frame.bottom	+= pt.y;
			RebuildFullRegion();
// TODO: uncomment later when you'll implement a queue in ServerWindow::SendMessgeToClient()
			//SendViewResizedMsg();

			newAction	= B_LAYER_MASK_RESIZE;
			break;
		}
		case B_LAYER_MASK_RESIZE:{
RBTRACE(("1) Action B_LAYER_MASK_RESIZE\n"));
			oldRegion	= _visible;

			BPoint		offset, rSize;
			BPoint		coords[2];

			ResizeOthers(pt.x, pt.y, coords, NULL);
			offset		= coords[0];
			rSize		= coords[1];
			newOffset	= offset + ptOffset;

			if(!(rSize.x == 0.0f && rSize.y == 0.0f)){
				_frame.OffsetBy(offset);
				_frame.right	+= rSize.x;
				_frame.bottom	+= rSize.y;
				RebuildFullRegion();
// TODO: uncomment later when you'll implement a queue in ServerWindow::SendMessgeToClient()
				//SendViewResizedMsg();
				
				newAction	= B_LAYER_MASK_RESIZE;
				newPt		= rSize;
				dummyNewLocation = newOffset;
			}
			else if (!(offset.x == 0.0f && offset.y == 0.0f)){
				pt			= newOffset;
				action		= B_LAYER_MOVE;
				newPt		= pt;
				goto RRLabel1;
			}
			else{
				pt			= ptOffset;
				action		= B_LAYER_MOVE;
				newPt		= pt;
				goto RRLabel1;
			}
			break;
		}
	}

	if (!IsHidden()){
		_visible			= _full;
		if (_parent){
			_visible.IntersectWith( &(_parent->_visible) );
				// exclude from parent's visible area.
			if ( !IsHidden() && _visible.CountRects() > 0)
				_parent->_visible.Exclude( &(_visible) );
		}
		_fullVisible		= _visible;
	}

		// Rebuild regions for children...
	for(Layer *lay = VirtualBottomChild(); lay != NULL; lay = VirtualUpperSibling())
	{
			lay->RebuildRegions(reg, newAction, newPt, newOffset);
	}

	if(!IsHidden())
	switch(action){
		case B_LAYER_NONE:{
			BRegion		r(_visible);
			if (oldRegion.CountRects() > 0)
				r.Exclude(&oldRegion);

			if(r.CountRects() > 0){
				gRedrawReg.Include(&r);
			}
			break;
		}
		case B_LAYER_MOVE:{
			BRegion		redrawReg;
			BRegion		*copyReg = new BRegion();
			BRegion		screenReg(fRootLayer->Bounds());

			oldRegion.OffsetBy(pt.x, pt.y);
			oldRegion.IntersectWith(&_fullVisible);

			*copyReg	= oldRegion;
			copyReg->IntersectWith(&screenReg);
			if(copyReg->CountRects() > 0 && !(pt.x == 0.0f && pt.y == 0.0f) ){
				copyReg->OffsetBy(-pt.x, -pt.y);
				BPoint		*point = new BPoint(pt);
				gCopyRegList.AddItem(copyReg);
				gCopyList.AddItem(point);
			}
			else{
				delete copyReg;
			}

			redrawReg	= _fullVisible;
			redrawReg.Exclude(&oldRegion);
			if(redrawReg.CountRects() > 0 && !(pt.x == 0.0f && pt.y == 0.0f) ){
				gRedrawReg.Include(&redrawReg);
			}

			break;
		}
		case B_LAYER_RESIZE:{
			BRegion		redrawReg;
			
			redrawReg	= _visible;
			redrawReg.Exclude(&oldRegion);
			if(redrawReg.CountRects() > 0){
				gRedrawReg.Include(&redrawReg);
			}

			break;
		}
		case B_LAYER_MASK_RESIZE:{
			BRegion		redrawReg;
			BRegion		*copyReg = new BRegion();

			oldRegion.OffsetBy(dummyNewLocation.x, dummyNewLocation.y);

			redrawReg	= _visible;
			redrawReg.Exclude(&oldRegion);
			if(redrawReg.CountRects() > 0)
			{
				gRedrawReg.Include(&redrawReg);
			}

			*copyReg	= _visible;
			copyReg->IntersectWith(&oldRegion);
			copyReg->OffsetBy(-dummyNewLocation.x, -dummyNewLocation.y);
			if(copyReg->CountRects() > 0
				&& !(dummyNewLocation.x == 0.0f && dummyNewLocation.y == 0.0f))
			{
				gCopyRegList.AddItem(copyReg);
				gCopyList.AddItem(new BPoint(dummyNewLocation));
			}

			break;
		}
		default:{
		}
	}
//if (IsHidden()){
//	_fullVisible.MakeEmpty();
//	_visible.MakeEmpty();
//}
#ifdef DEBUG_LAYER_REBUILD
 printf("\n ======= Layer(%s)::RR finals ======\n", GetName());
 oldRegion.PrintToStream();
 _full.PrintToStream();
 _fullVisible.PrintToStream();
 _visible.PrintToStream();
 printf("==========RedrawReg===========\n");
 gRedrawReg.PrintToStream();
 printf("=====================\n");
#endif
STRACE(("Layer(%s)::RebuildRegions() END\n", GetName()));
}

void Layer::StartRebuildRegions( const BRegion& reg, Layer *target, uint32 action, BPoint& pt)
{
STRACE(("Layer(%s)::StartRebuildRegions() START\n", GetName()));
	if(!_parent)
		_fullVisible	= _full;

	BRegion		oldVisible = _visible;

	_visible		= _fullVisible;

		// Rebuild regions for children...
	for(Layer *lay = VirtualBottomChild(); lay != NULL; lay = VirtualUpperSibling())
	{
		if (lay == target){
			lay->RebuildRegions(reg, action, pt, BPoint(0.0f, 0.0f));
		}
		else{
			lay->RebuildRegions(reg, B_LAYER_NONE, pt, BPoint(0.0f, 0.0f));
		}
	}
#ifdef DEBUG_LAYER_REBUILD
 printf("\n ===!=== Layer(%s)::SRR finals ===!===\n", GetName());
 _full.PrintToStream();
 _fullVisible.PrintToStream();
 _visible.PrintToStream();
 oldVisible.PrintToStream();
 printf("=====!=====RedrawReg=====!=====\n");
 gRedrawReg.PrintToStream();
 printf("=====================\n");
#endif

	BRegion		redrawReg(_visible);
		// if this is the first time
	if (oldVisible.CountRects() > 0){
		redrawReg.Exclude(&oldVisible);
	}
	gRedrawReg.Include(&redrawReg);
	
#ifdef DEBUG_LAYER_REBUILD
 printf("Layer(%s)::StartRebuildREgions() ended! Redraw Region:\n", GetName());
 gRedrawReg.PrintToStream();
 printf("\n");
 printf("Layer(%s)::StartRebuildREgions() ended! Copy Region:\n", GetName());
 for(int32 k=0; k<gCopyRegList.CountItems(); k++){
	((BRegion*)(gCopyRegList.ItemAt(k)))->PrintToStream();
	((BPoint*)(gCopyList.ItemAt(k)))->PrintToStream();
 }
 printf("\n");
#endif

STRACE(("Layer(%s)::StartRebuildRegions() END\n", GetName()));
}

void Layer::MoveBy(float x, float y)
{
STRACE(("Layer(%s)::MoveBy() START\n", GetName()));
	if(!_parent){
		debugger("ERROR: in Layer::MoveBy()! - No parent!\n");
		return;
	}

	BPoint		pt(x,y);	
	BRect		rect(_full.Frame().OffsetByCopy(pt));

	_parent->StartRebuildRegions(BRegion(rect), this, B_LAYER_MOVE, pt);
	
	fDriver->CopyRegionList(&gCopyRegList, &gCopyList, gCopyRegList.CountItems(), &_fullVisible);

	_parent->Redraw(gRedrawReg, this);

	EmptyGlobals();
STRACE(("Layer(%s)::MoveBy() END\n", GetName()));
}

void Layer::EmptyGlobals(){
	void	*item;

	gRedrawReg.MakeEmpty();
	while((item = gCopyRegList.RemoveItem((int32)0))){
		delete (BRegion*)item;
	}

	while((item = gCopyList.RemoveItem((int32)0))){
		delete (BPoint*)item;
	}
}

uint32 Layer::ResizeOthers(float x, float y, BPoint coords[], BPoint *ptOffset){
STRACE(("Layer(%s)::ResizeOthers() START\n", GetName()));
	uint32		rmask		= _resize_mode;
		// offset
	coords[0].x		= 0.0f;
	coords[0].y		= 0.0f;
		// resize by width/height
	coords[1].x		= 0.0f;
	coords[1].y		= 0.0f;

	if ((rmask & 0x00000f00UL)>>8 == _VIEW_LEFT_
			&& (rmask & 0x0000000fUL)>>0 == _VIEW_RIGHT_){
		coords[1].x		= x;
	}
	else if ((rmask & 0x00000f00UL)>>8 == _VIEW_LEFT_){
	}
	else if ((rmask & 0x0000000fUL)>>0 == _VIEW_RIGHT_){
		coords[0].x		= x;
	}
	else if ((rmask & 0x00000f00UL)>>8 == _VIEW_CENTER_){
		coords[0].x		= x/2;
	}
	else { // illegal flag. Do nothing.
	}


	if ((rmask & 0x0000f000UL)>>12 == _VIEW_TOP_
			&& (rmask & 0x000000f0UL)>>4 == _VIEW_BOTTOM_){
		coords[1].y		= y;
	}
	else if ((rmask & 0x0000f000UL)>>12 == _VIEW_TOP_){
	}
	else if ((rmask & 0x000000f0UL)>>4 == _VIEW_BOTTOM_){
		coords[0].y		= y;
	}
	else if ((rmask & 0x0000f000UL)>>12 == _VIEW_CENTER_){
		coords[0].y		= y/2;
	}
	else { // illegal flag. Do nothing.
	}

STRACE(("Layer(%s)::ResizeOthers() END\n", GetName()));
	return 0UL;
}

void Layer::ResizeBy(float x, float y)
{
STRACE(("Layer(%s)::ResizeBy() START\n", GetName()));
	if(!_parent){
		printf("ERROR: in Layer::MoveBy()! - No parent!\n");
		return;
	}

	BPoint		pt(x,y);	
	BRect		rect(_full.Frame());
	rect.right	+= x;
	rect.bottom	+= y;

	_parent->StartRebuildRegions(BRegion(rect), this, B_LAYER_RESIZE, pt);
	
	fDriver->CopyRegionList(&gCopyRegList, &gCopyList, gCopyRegList.CountItems(), &_fullVisible);

	_parent->Redraw(gRedrawReg, this);

	EmptyGlobals();
STRACE(("Layer(%s)::ResizeBy() END\n", GetName()));
}

void Layer::PrintToStream(void)
{
	printf("\n----------- Layer %s -----------\n",_name->String());
	printf("\t Parent: %s\n", _parent? _parent->_name->String():"NULL");
	printf("\t us: %s\t ls: %s\n",
				_uppersibling? _uppersibling->_name->String():"NULL",
				_lowersibling? _lowersibling->_name->String():"NULL");
	printf("\t topChild: %s\t bottomChild: %s\n",
				_topchild? _topchild->_name->String():"NULL",
				_bottomchild? _bottomchild->_name->String():"NULL");
						
	printf("Frame: (%f, %f, %f, %f)", _frame.left, _frame.top, _frame.right, _frame.bottom);
	printf("Token: %ld\n",_view_token);
	printf("Hidden - direct: %s\n", _hidden?"true":"false");
	printf("Hidden - indirect: %s\n", IsHidden()?"true":"false");
	printf("ResizingMode: %lx\n", _resize_mode);
	printf("Flags: %lx\n", _flags);

	if (_layerdata)
		_layerdata->PrintToStream();
	else
		printf(" NO LayerData valid pointer\n");
}

void Layer::PrintNode(void)
{
	printf("-----------\nLayer %s\n",_name->String());
	if(_parent)
		printf("Parent: %s (%p)\n",_parent->_name->String(), _parent);
	else
		printf("Parent: NULL\n");
	if(_uppersibling)
		printf("Upper sibling: %s (%p)\n",_uppersibling->_name->String(), _uppersibling);
	else
		printf("Upper sibling: NULL\n");
	if(_lowersibling)
		printf("Lower sibling: %s (%p)\n",_lowersibling->_name->String(), _lowersibling);
	else
		printf("Lower sibling: NULL\n");
	if(_topchild)
		printf("Top child: %s (%p)\n",_topchild->_name->String(), _topchild);
	else
		printf("Top child: NULL\n");
	if(_bottomchild)
		printf("Bottom child: %s (%p)\n",_bottomchild->_name->String(), _bottomchild);
	else
		printf("Bottom child: NULL\n");
	printf("Visible Areas: "); _visible.PrintToStream();
}

void Layer::PrintTree(){
	printf("\n Tree structure:\n");
	printf("\t%s\t%s\n", GetName(), IsHidden()? "Hidden": "NOT hidden");
	for(Layer *lay = VirtualBottomChild(); lay != NULL; lay = VirtualUpperSibling())
	{
		printf("\t%s\t%s\n", lay->GetName(), lay->IsHidden()? "Hidden": "NOT hidden");
	}
}

BRect Layer::ConvertToParent(BRect rect)
{
	return (rect.OffsetByCopy(_frame.LeftTop()));
}

BRegion Layer::ConvertToParent(BRegion *reg)
{
	BRegion newreg;
	for(int32 i=0; i<reg->CountRects(); i++)
		newreg.Include( (reg->RectAt(i)).OffsetByCopy(_frame.LeftTop()) );
	return newreg;
}

BRect Layer::ConvertFromParent(BRect rect)
{
	return (rect.OffsetByCopy(_frame.left*-1,_frame.top*-1));
}

BRegion Layer::ConvertFromParent(BRegion *reg)
{
	BRegion newreg;
	for(int32 i=0; i<reg->CountRects();i++)
		newreg.Include((reg->RectAt(i)).OffsetByCopy(_frame.left*-1,_frame.top*-1));
	return newreg;
}

BRegion Layer::ConvertToTop(BRegion *reg)
{
	BRegion newreg;
	for(int32 i=0; i<reg->CountRects();i++)
		newreg.Include(ConvertToTop(reg->RectAt(i)));
	return newreg;
}

BRect Layer::ConvertToTop(BRect rect)
{
	if (_parent!=NULL)
		return(_parent->ConvertToTop(rect.OffsetByCopy(_frame.LeftTop())) );
	else
		return(rect);
}

BRegion Layer::ConvertFromTop(BRegion *reg)
{
	BRegion newreg;
	for(int32 i=0; i<reg->CountRects();i++)
		newreg.Include(ConvertFromTop(reg->RectAt(i)));
	return newreg;
}

BRect Layer::ConvertFromTop(BRect rect)
{
	if (_parent!=NULL)
		return(_parent->ConvertFromTop(rect.OffsetByCopy(_frame.LeftTop().x*-1,
			_frame.LeftTop().y*-1)) );
	else
		return(rect);
}

void Layer::SendViewResizedMsg(){
	if( _serverwin && _flags & B_FRAME_EVENTS )
	{
		BMessage		msg;
		msg.what		= B_VIEW_RESIZED;
		msg.AddInt64( "when", real_time_clock_usecs() );
		msg.AddInt32( "_token", _view_token );
		msg.AddFloat( "width", _frame.Width() );
		msg.AddFloat( "height", _frame.Height() );
			// no need for that... it's here because of backward compatibility
		msg.AddPoint( "where", _frame.LeftTop() );
		
		_serverwin->SendMessageToClient( &msg );
	}
}

void Layer::SendViewMovedMsg(){
	if( _serverwin && _flags & B_FRAME_EVENTS )
	{
		BMessage		msg;
		msg.what		= B_VIEW_MOVED;
		msg.AddInt64( "when", real_time_clock_usecs() );
		msg.AddInt32( "_token", _view_token );
		msg.AddPoint( "where", _frame.LeftTop() );
						
		_serverwin->SendMessageToClient( &msg );
	}
}

Layer* Layer::VirtualTopChild() const{
	fCurrent	= _topchild;
	return fCurrent;
}

Layer* Layer::VirtualLowerSibling() const{
	fCurrent	= fCurrent->_lowersibling;
	return fCurrent;
}

Layer* Layer::VirtualUpperSibling() const{
	fCurrent	= fCurrent->_uppersibling;
	return fCurrent;
}

Layer* Layer::VirtualBottomChild() const{
	fCurrent	= _bottomchild;
	return fCurrent;
}
