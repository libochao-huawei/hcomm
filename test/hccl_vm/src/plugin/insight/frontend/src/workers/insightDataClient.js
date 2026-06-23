let workerSingleton = null
let nextRequestId = 0
const pending = new Map()

function ensureWorker() {
  if (workerSingleton) return workerSingleton
  workerSingleton = new Worker(new URL('./insightDataWorker.js', import.meta.url), { type: 'module' })
  workerSingleton.addEventListener('message', (event) => {
    const { id, ok, result, error } = event.data ?? {}
    const entry = pending.get(id)
    if (!entry) return
    pending.delete(id)
    if (ok) entry.resolve(result)
    else entry.reject(new Error(error || 'Worker request failed'))
  })
  workerSingleton.addEventListener('error', () => {
    pending.forEach(({ reject }) => reject(new Error('Insight worker crashed')))
    pending.clear()
  })
  return workerSingleton
}

export function postWorker(kind, payload, transferables = []) {
  const worker = ensureWorker()
  const id = ++nextRequestId
  return new Promise((resolve, reject) => {
    pending.set(id, { resolve, reject })
    worker.postMessage({ id, kind, payload }, transferables)
  })
}

export function rehydrateMemviewIndex(serialized) {
  if (!serialized) return null
  return {
    totalSteps: serialized.totalSteps,
    memoryTaskIds: serialized.memoryTaskIds,
    stepByNodeId: new Map(serialized.stepByNodeId),
    bufferPresence: new Map(serialized.bufferPresence),
    bufferChangedFlags: new Map(serialized.bufferChangedFlags),
    bufferOps: serialized.bufferOps,
    taskOps: serialized.taskOps ?? null,
    bufferOpsByNode: new Map(serialized.bufferOpsByNode),
  }
}
