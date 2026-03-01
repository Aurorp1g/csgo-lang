package scheduler

import (
	"context"
	"fmt"
	"runtime"
	"sync"
	"sync/atomic"
	"time"
)

type Task struct {
	ID       int64
	Fn       func() interface{}
	Result   interface{}
	Error    error
	Done     chan struct{}
	State    TaskState
	Priority int
	Created  time.Time
	Started  time.Time
	Ended    time.Time
}

type TaskState int

const (
	TaskReady TaskState = iota
	TaskRunning
	TaskCompleted
	TaskFailed
	TaskCancelled
)

type Scheduler struct {
	workers   int
	tasks     chan *Task
	ready     []*Task
	mu        sync.Mutex
	wg        sync.WaitGroup
	ctx       context.Context
	cancel    context.CancelFunc
	running   int64
	completed int64
	failed    int64
	stats     *Stats
	debug     bool
}

type Stats struct {
	TasksCreated   int64
	TasksRan       int64
	TasksSucceeded int64
	TasksFailed    int64
	WorkersActive  int64
	QueueDepth     int64
}

func New(workers int) *Scheduler {
	if workers <= 0 {
		workers = 1
	}
	ctx, cancel := context.WithCancel(context.Background())
	return &Scheduler{
		workers: workers,
		tasks:   make(chan *Task, 1024),
		ready:   make([]*Task, 0),
		ctx:     ctx,
		cancel:  cancel,
		stats:   &Stats{},
		debug:   false,
	}
}

func (s *Scheduler) SetDebug(debug bool) {
	s.debug = debug
}

func (s *Scheduler) Start() {
	for i := 0; i < s.workers; i++ {
		s.wg.Add(1)
		go s.worker(i)
	}
	if s.debug {
		fmt.Printf("[Scheduler] Started with %d workers\n", s.workers)
	}
}

func (s *Scheduler) Stop() {
	s.cancel()
	s.wg.Wait()
	if s.debug {
		fmt.Printf("[Scheduler] Stopped\n")
	}
}

func (s *Scheduler) worker(id int) {
	defer s.wg.Done()

	for {
		select {
		case <-s.ctx.Done():
			if s.debug {
				fmt.Printf("[Scheduler] Worker %d shutting down\n", id)
			}
			return
		case task := <-s.tasks:
			s.runTask(task, id)
		default:
			s.mu.Lock()
			if len(s.ready) > 0 {
				task := s.ready[0]
				s.ready = s.ready[1:]
				s.mu.Unlock()
				s.runTask(task, id)
			} else {
				s.mu.Unlock()
				time.Sleep(100 * time.Microsecond)
			}
		}
	}
}

func (s *Scheduler) runTask(task *Task, workerID int) {
	if s.debug {
		fmt.Printf("[Scheduler] Worker %d running task %d\n", workerID, task.ID)
	}

	task.State = TaskRunning
	task.Started = time.Now()
	atomic.AddInt64(&s.running, 1)
	atomic.AddInt64(&s.stats.TasksRan, 1)

	func() {
		defer func() {
			if r := recover(); r != nil {
				task.Error = fmt.Errorf("panic: %v", r)
				task.State = TaskFailed
				atomic.AddInt64(&s.failed, 1)
				atomic.AddInt64(&s.stats.TasksFailed, 1)
			}
			close(task.Done)
		}()
		task.Result = task.Fn()
	}()

	task.Ended = time.Now()
	task.State = TaskCompleted
	atomic.AddInt64(&s.running, -1)
	atomic.AddInt64(&s.completed, 1)

	if s.debug {
		fmt.Printf("[Scheduler] Task %d completed in %v\n", task.ID, task.Ended.Sub(task.Started))
	}
}

func (s *Scheduler) Submit(fn func() interface{}) *Task {
	task := &Task{
		ID:       atomic.AddInt64(&s.stats.TasksCreated, 1),
		Fn:       fn,
		Done:     make(chan struct{}),
		State:    TaskReady,
		Created:  time.Now(),
		Priority: 0,
	}

	select {
	case s.tasks <- task:
		if s.debug {
			fmt.Printf("[Scheduler] Task %d submitted\n", task.ID)
		}
	default:
		s.mu.Lock()
		s.ready = append(s.ready, task)
		s.mu.Unlock()
		if s.debug {
			fmt.Printf("[Scheduler] Task %d queued (queue size: %d)\n", task.ID, len(s.ready))
		}
	}

	return task
}

func (s *Scheduler) SubmitWithPriority(fn func() interface{}, priority int) *Task {
	task := &Task{
		ID:       atomic.AddInt64(&s.stats.TasksCreated, 1),
		Fn:       fn,
		Done:     make(chan struct{}),
		State:    TaskReady,
		Created:  time.Now(),
		Priority: priority,
	}

	s.mu.Lock()
	inserted := false
	for i, t := range s.ready {
		if task.Priority > t.Priority {
			s.ready = append(s.ready[:i], append([]*Task{task}, s.ready[i:]...)...)
			inserted = true
			break
		}
	}
	if !inserted {
		s.ready = append(s.ready, task)
	}
	s.mu.Unlock()

	if s.debug {
		fmt.Printf("[Scheduler] Task %d submitted with priority %d\n", task.ID, priority)
	}

	return task
}

func (s *Scheduler) Wait(task *Task) (interface{}, error) {
	<-task.Done
	return task.Result, task.Error
}

func (s *Scheduler) WaitAll() {
	for {
		s.mu.Lock()
		hasWork := len(s.ready) > 0 || atomic.LoadInt64(&s.running) > 0
		s.mu.Unlock()
		if !hasWork {
			return
		}
		time.Sleep(10 * time.Millisecond)
	}
}

func (s *Scheduler) GetStats() Stats {
	s.mu.Lock()
	defer s.mu.Unlock()
	stats := *s.stats
	stats.WorkersActive = int64(s.workers)
	stats.QueueDepth = int64(len(s.ready))
	return stats
}

type GoroutinePool struct {
	scheduler *Scheduler
	stackSize int
}

func NewPool(workers int) *GoroutinePool {
	return &GoroutinePool{
		scheduler: New(workers),
		stackSize: 0,
	}
}

func (p *GoroutinePool) Start() {
	p.scheduler.Start()
}

func (p *GoroutinePool) Stop() {
	p.scheduler.Stop()
}

func (p *GoroutinePool) Go(fn func() interface{}) *Task {
	return p.scheduler.Submit(fn)
}

func (p *GoroutinePool) GoWithPriority(fn func() interface{}, priority int) *Task {
	return p.scheduler.SubmitWithPriority(fn, priority)
}

func (p *GoroutinePool) Wait(task *Task) (interface{}, error) {
	return p.scheduler.Wait(task)
}

func (p *GoroutinePool) WaitAll() {
	p.scheduler.WaitAll()
}

var defaultPool *GoroutinePool

func init() {
	defaultPool = NewPool(runtime.NumCPU())
	defaultPool.Start()
}

func Go(fn func() interface{}) *Task {
	return defaultPool.Go(fn)
}

func GoWithPriority(fn func() interface{}, priority int) *Task {
	return defaultPool.GoWithPriority(fn, priority)
}

func Wait(task *Task) (interface{}, error) {
	return defaultPool.Wait(task)
}

func WaitAll() {
	defaultPool.WaitAll()
}

func Stop() {
	defaultPool.Stop()
}
