/*
 * uielement.h
 *
 * Author:
 *   Miguel de Icaza (miguel@novell.com)
 *
 * Copyright 2007 Novell, Inc. (http://www.novell.com)
 *
 * See the LICENSE file included with the distribution for details.
 * 
 */

#ifndef __MOON_UIELEMENT_H__
#define __MOON_UIELEMENT_H__

#include "visual.h"
#include "point.h"
#include "rect.h"

class Surface;

struct KeyboardEventArgs {
	int state;
	int platformcode;
	int key;
};

struct MouseEventArgs {
	int state;
	double x;
	double y;
};

class UIElement : public Visual {
	Brush *opacityMask;

	double total_opacity;
protected:
	Rect IntersectBoundsWithClipPath (Rect bounds, bool transform);
	void RenderClipPath (cairo_t *cr);
public:
	UIElement ();
	virtual ~UIElement ();
	virtual Type::Kind GetObjectType () { return Type::UIELEMENT; };

	UIElement *parent;

	int dirty_flags;
	Rect dirty_rect;
	Rect children_dirty_rect;

	int DumpHierarchy (UIElement *obj);

	enum UIElementFlags {
		IS_LOADED        = 0x01,

		// these two flags correspond to the 2 states of VisibilityProperty
		RENDER_VISIBLE   = 0x02,

		// the HitTestVisible property
		HIT_TEST_VISIBLE = 0x04,

		SHAPE_EMPTY      = 0x10,	// there's is nothing to draw, the cached path may be NULL
		SHAPE_NORMAL     = 0x20,	// normal drawing
		SHAPE_DEGENERATE = 0x40,	// degenerate drawing, use the Stroke brush for filling
		SHAPE_MASK       = 0x70
	};
	
	int flags;

	// The computed bounding box
	Rect bounds;

	// Absolute affine transform, precomputed with all of its data
	cairo_matrix_t absolute_xform;

	// the transform to be multiplied by our parent transform to compute absolute_xform
	cairo_matrix_t local_transform;

	cairo_matrix_t parent_transform;

	Point transform_origin;

	virtual bool EnableAntiAlias() { return true; }

	//
	// UpdateTotalOpacity:
	//   Updates the opacity on this item based on its parent's
	//   opacity as well as the value of its OpacityProperty.
	//
	virtual void UpdateTotalOpacity ();

	void ComputeTotalOpacity ();

	// GetTotalOpacity
	//   Get the cumulative opacity of this element, including all it's parents
	double GetTotalOpacity () { return total_opacity; }

	//
	// UpdateTransform:
	//   Updates the absolute_xform for this item
	//
	virtual void UpdateTransform ();

	void ComputeLocalTransform ();
	void ComputeTransform ();

	//
	// GetVisible:
	//   Returns true if the Visibility property of this item is "Visible", and false otherwise
	//
	bool GetVisible () { return (flags & UIElement::RENDER_VISIBLE) != 0; }

	//
	// GetHitTestVisible:
	//   Returns true if the IsHitTestVisible property of this item true, and false otherwise
	//
	bool GetHitTestVisible () { return (flags & UIElement::HIT_TEST_VISIBLE) != 0; }

	//
	// Render: 
	//   Renders the given @item on the @surface.  the area that is
	//   exposed is delimited by x, y, width, height
	//
	virtual void Render (cairo_t *cr, int x, int y, int width, int height);

	// a non virtual method for use when we want to wrap render
	// with debugging and/or timing info
	void DoRender (cairo_t *cr, int x, int y, int width, int height);

	//
	// GetSizeForBrush:
	//   Gets the size of the area to be painted by a Brush (needed for image/video scaling)
	virtual void GetSizeForBrush (cairo_t *cr, double *width, double *height);

	//
	// UpdateBounds:
	//   Recomputes the bounds of this element, and if they're
	//   different chains up to its parent telling it to update
	//   its bounds.
	//
	void UpdateBounds (bool force_redraw_of_new_bounds = false);

	// 
	// ComputeBounds:
	//   Updates the bounding box for the given item, this uses the parent
	//   chain to compute the composite affine.
	//
	// Output:
	//   item->bounds is updated
	// 
	virtual void ComputeBounds ();

	// 
	// GetBounds:
	//   returns the current bounding box for the given item.
	// 
	virtual Rect GetBounds () { return bounds; }

	//
	// GetTransformFor
	//   Obtains the affine transform for the given child, this is
	//   implemented by containers

	virtual void GetTransformFor (UIElement *item, cairo_matrix_t *result);

	//
	// Recomputes the bounding box, requests redraws, 
	// the parameter determines if we should also update the transformation
	//
	void FullInvalidate (bool render_xform);

	virtual bool InsideObject (cairo_t *cr, double x, double y);

	//
	// Checks if the point is inside the Clip region.
	// Returns true if no Clip region is defined
	// (which is actually an infinitely big Clip region).
	// 
	bool InsideClip (cairo_t *cr, double x, double y);

	void ChildInvalidated (Rect r);

	//
	// Invalidates a subrectangle of this element
	//
	void Invalidate (Rect r);
	
	//
	// Invalidates the items bounding rectangle on its surface
	//
	void Invalidate ();

	//
	// GetTransformOrigin:
	//   Returns the transformation origin based on  of the item and the
	//   xform_origin
	virtual Point GetTransformOrigin () {
		return Point (0, 0);
	}

	//
	// HandleMotion:
	//   handles an mouse motion event, and dispatches it to anyone that
	//   might want it.
	//
	virtual void HandleMotion (cairo_t *cr, int state, double x, double y, MouseCursor *cursor);

	//
	// HandleButtonPress:
	//   handles the button press event and dispatches it to all
	//   the objects that might be interested in it (nested
	//   objects).
	//
	virtual void HandleButtonPress (cairo_t *cr, int state, double x, double y);

	//
	// HandleButtonRelease:
	//   handles the button release event and dispatches it to all
	//   the objects that might be interested in it (nested
	//   objects).
	//
	virtual void HandleButtonRelease (cairo_t *cr, int state, double x, double y);

	//
	// HandleKeyDown:
	//
	virtual void HandleKeyDown (cairo_t *cr, int state, Key key, int platform_key_code);

	//
	// HandleKeyDown:
	//
	virtual void HandleKeyUp (cairo_t *cr, int state, Key key, int platform_key_code);

	//
	// Enter:
	//   Invoked when the mouse first enters this given object
	//
	virtual void Enter (cairo_t *cr, int state, double x, double y);
	
	//
	// Leave:
	//   Invoke when the mouse leaves this given object
	//
	virtual void Leave ();


	//
	// CaptureMouse:
	//
	//    Attempts to capture the mouse.  If successful, all mouse
	//    events will be transmitted directly to this element.
	//    Leave/Enter events will no longer be sent.
	//
	bool CaptureMouse ();

	//
	// ReleaseMouseCapture:
	//
	//    Attempts to release the mouse.  If successful, any
	//    applicable Leave/Enter events for the current mouse
	//    position will be sent.
	//
	void ReleaseMouseCapture ();

	virtual void OnLoaded ();

	virtual void OnPropertyChanged (DependencyProperty *prop);
	virtual void OnSubPropertyChanged (DependencyProperty *prop, DependencyProperty *subprop);

	Point GetRenderTransformOrigin () {
		Value *vu = GetValue (UIElement::RenderTransformOriginProperty);
		if (vu)
			return *vu->AsPoint ();
		return Point (0, 0);
	}

	static DependencyProperty* RenderTransformProperty;
	static DependencyProperty* OpacityProperty;
	static DependencyProperty* ClipProperty;
	static DependencyProperty* OpacityMaskProperty;
	static DependencyProperty* RenderTransformOriginProperty;
	static DependencyProperty* CursorProperty;
	static DependencyProperty* IsHitTestVisibleProperty;
	static DependencyProperty* VisibilityProperty;
	static DependencyProperty* ResourcesProperty;
	static DependencyProperty* TagProperty;
	static DependencyProperty* TriggersProperty;
	static DependencyProperty* ZIndexProperty;

	// Events you can AddHandler to
	static int LoadedEvent;
	static int MotionEvent;
	static int ButtonPressEvent;
	static int ButtonReleaseEvent;
	static int KeyDownEvent;
	static int KeyUpEvent;
	static int EnterEvent;
	static int LeaveEvent;
	static int InvalidatedEvent;
	static int GotFocusEvent;
	static int LostFocusEvent;
};

G_BEGIN_DECLS

UIElement *uielement_new                  (void);

Surface   *uielement_get_surface          (UIElement *item);
void       uielement_invalidate           (UIElement *item);
void       uielement_update_bounds        (UIElement *item);
void       uielement_set_transform        (UIElement *item, double *transform);
void       uielement_set_transform_origin (UIElement *item, Point p);
         
void       uielement_set_render_transform (UIElement *item, Transform *transform);
void       uielement_get_render_affine    (UIElement *item, cairo_matrix_t *result);
         
double     uielement_get_opacity          (UIElement *item);
void       uielement_set_opacity          (UIElement *item, double opacity);
         
Brush     *uielement_get_opacity_mask     (UIElement *item);
void       uielement_transform_point      (UIElement *item, double *x, double *y);
UIElement *uielement_get_parent           (UIElement *item);

bool       uielement_capture_mouse         (UIElement *item);
void       uielement_release_mouse_capture (UIElement *item);

void     uielement_init ();
G_END_DECLS

#endif /* __MOON_UIELEMENT_H__ */
