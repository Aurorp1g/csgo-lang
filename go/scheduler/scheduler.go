// Package scheduler 提供 CSGO 编程语言的协程调度器
//
// 本包实现了轻量级协程调度器，基于 Go 的 GMP 模型设计。
// 支持任务队列、工作池、优先级调度等功能。
//
// 核心类型：
//   - Task: 协程任务结构
//   - Scheduler: 调度器主结构
//   - Stats: 调度统计信息
//
// 功能特性：
//   - 工作池模式
//   - 任务优先级调度
//   - 上下文取消
//   - 统计信息收集
//   - 调试支持
//
// 设计参考：
//   - Go runtime: GMP 调度模型
//   - Python asyncio: 协程调度
package scheduler

import (
	"context"
	"fmt"
	"runtime"
	"sync"
	"sync/atomic"
	"time"
)

// Task 表示一个协程任务
//
// 包含任务的元数据、状态和执行结果。
type Task struct {
	ID       int64              // 任务唯一标识
	Fn       func() interface{} // 任务执行函数
	Result   interface{}        // 任务执行结果
	Error    error              // 任务执行错误
	Done     chan struct{}      // 任务完成信号
	State    TaskState          // 任务状态
	Priority int                // 任务优先级
	Created  time.Time          // 创建时间
	Started  time.Time          // 开始时间
	Ended    time.Time          // 结束时间
}

// TaskState 表示任务状态
type TaskState int

// 任务状态常量
const (
	TaskReady     TaskState = iota // 任务就绪
	TaskRunning                    // 任务运行中
	TaskCompleted                  // 任务完成
	TaskFailed                     // 任务失败
	TaskCancelled                  // 任务取消
)

// Scheduler 协程调度器
//
// 基于工作池模式的轻量级调度器。
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

// Stats 调度统计信息
//
// 收集调度器运行时的各种统计信息。
type Stats struct {
	TasksCreated   int64 // 创建的任务总数
	TasksRan       int64 // 已运行的任务数
	TasksSucceeded int64 // 成功完成的任务数
	TasksFailed    int64 // 失败的任务数
	WorkersActive  int64 // 当前活跃的工作协程数
	QueueDepth     int64 // 任务队列深度
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
