package gc

import (
	"runtime"
	"sync"
	"sync/atomic"
	"time"
	"unsafe"
)

type Object struct {
	Marked    bool
	Finalizer func(*Object)
	Type      int
	Data      interface{}
	RefCount  int32
	Size      int
	Next      *Object
}

type GC struct {
	roots      []*Object
	objects    *Object
	mu         sync.RWMutex
	threshold  uint64
	allocBytes uint64
	enabled    bool
	debug      bool
}

var defaultGC *GC

func init() {
	defaultGC = New(1024 * 1024)
	runtime.SetFinalizer(nil, nil)
}

func New(threshold uint64) *GC {
	return &GC{
		threshold: threshold,
		enabled:   true,
		debug:     false,
	}
}

func (g *GC) Enable() {
	g.enabled = true
}

func (g *GC) Disable() {
	g.enabled = false
}

func (g *GC) SetThreshold(threshold uint64) {
	g.threshold = threshold
}

func (g *GC) SetDebug(debug bool) {
	g.debug = debug
}

func (g *GC) AddRoot(obj *Object) {
	g.mu.Lock()
	defer g.mu.Unlock()
	g.roots = append(g.roots, obj)
}

func (g *GC) RemoveRoot(obj *Object) {
	g.mu.Lock()
	defer g.mu.Unlock()
	for i, r := range g.roots {
		if r == obj {
			g.roots = append(g.roots[:i], g.roots[i+1:]...)
			return
		}
	}
}

func (g *GC) Allocate(typ int, size int, data interface{}) *Object {
	if !g.enabled {
		obj := &Object{
			Type: typ,
			Size: size,
			Data: data,
		}
		atomic.AddInt32(&obj.RefCount, 1)
		return obj
	}

	atomic.AddUint64(&g.allocBytes, uint64(size))

	if g.allocBytes > g.threshold {
		g.Collect()
	}

	obj := &Object{
		Type: typ,
		Size: size,
		Data: data,
	}

	g.mu.Lock()
	obj.Next = g.objects
	g.objects = obj
	g.mu.Unlock()

	atomic.AddInt32(&obj.RefCount, 1)

	return obj
}

func (g *GC) Retain(obj *Object) {
	if obj == nil || !g.enabled {
		return
	}
	atomic.AddInt32(&obj.RefCount, 1)
}

func (g *GC) Release(obj *Object) {
	if obj == nil || !g.enabled {
		return
	}
	refCount := atomic.AddInt32(&obj.RefCount, -1)
	if refCount <= 0 {
		g.free(obj)
	}
}

func (g *GC) free(obj *Object) {
	if g.debug {
		println("[GC] Freeing object:", obj)
	}
	atomic.AddUint64(&g.allocBytes, ^uint64(obj.Size-1))

	if obj.Finalizer != nil {
		obj.Finalizer(obj)
	}

	if obj.Data != nil {
		switch v := obj.Data.(type) {
		case []interface{}:
			for _, item := range v {
				if o, ok := item.(*Object); ok {
					g.Release(o)
				}
			}
		case map[interface{}]interface{}:
			for _, v := range v {
				if o, ok := v.(*Object); ok {
					g.Release(o)
				}
			}
		case *Object:
			g.Release(v)
		}
	}
}

func (g *GC) Collect() {
	if g.debug {
		println("[GC] Starting collection...")
		start := time.Now()
		defer func() {
			println("[GC] Collection done in", time.Since(start))
		}()
	}

	g.mark()
	g.sweep()
}

func (g *GC) mark() {
	g.mu.RLock()
	roots := make([]*Object, len(g.roots))
	copy(roots, g.roots)
	g.mu.RUnlock()

	for _, root := range roots {
		g.markObject(root)
	}
}

func (g *GC) markObject(obj *Object) {
	if obj == nil || obj.Marked {
		return
	}

	obj.Marked = true

	switch v := obj.Data.(type) {
	case *Object:
		g.markObject(v)
	case []interface{}:
		for _, item := range v {
			if o, ok := item.(*Object); ok {
				g.markObject(o)
			}
		}
	case map[interface{}]interface{}:
		for _, v := range v {
			if o, ok := v.(*Object); ok {
				g.markObject(o)
			}
		}
	case []byte:
	case string:
	default:
		if ptr, ok := v.(unsafe.Pointer); ok {
			g.markObject((*Object)(ptr))
		}
	}
}

func (g *GC) sweep() {
	var prev *Object

	g.mu.Lock()
	obj := g.objects
	g.mu.Unlock()

	for obj != nil {
		if !obj.Marked {
			if prev != nil {
				prev.Next = obj.Next
			} else {
				g.objects = obj.Next
			}
			next := obj.Next
			g.free(obj)
			obj = next
		} else {
			obj.Marked = false
			prev = obj
			obj = obj.Next
		}
	}

	atomic.StoreUint64(&g.allocBytes, 0)
}

func (g *GC) Stats() (allocated uint64, threshold uint64, objectCount int) {
	allocated = atomic.LoadUint64(&g.allocBytes)
	threshold = g.threshold

	g.mu.RLock()
	defer g.mu.RUnlock()
	count := 0
	for obj := g.objects; obj != nil; obj = obj.Next {
		count++
	}
	objectCount = count

	return
}

func Enable() {
	if defaultGC != nil {
		defaultGC.Enable()
	}
}

func Disable() {
	if defaultGC != nil {
		defaultGC.Disable()
	}
}

func Collect() {
	if defaultGC != nil {
		defaultGC.Collect()
	}
}

func Allocate(typ int, size int, data interface{}) *Object {
	if defaultGC != nil {
		return defaultGC.Allocate(typ, size, data)
	}
	return nil
}

func Retain(obj *Object) {
	if defaultGC != nil {
		defaultGC.Retain(obj)
	}
}

func Release(obj *Object) {
	if defaultGC != nil {
		defaultGC.Release(obj)
	}
}

func AddRoot(obj *Object) {
	if defaultGC != nil {
		defaultGC.AddRoot(obj)
	}
}

func RemoveRoot(obj *Object) {
	if defaultGC != nil {
		defaultGC.RemoveRoot(obj)
	}
}
