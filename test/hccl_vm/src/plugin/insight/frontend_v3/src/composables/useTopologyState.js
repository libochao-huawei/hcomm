import { computed, ref, shallowRef, watch } from 'vue'
import {
  fetchTopologyList,
  fetchClusterTopology,
  parseClusterTopology,
  parseServerTopoConfig,
} from '../utils/topologyData'

const topologyList = ref([])
const selectedTopoName = ref('')
const rawClusterData = shallowRef(null)
const loading = ref(false)
const error = ref('')
const topoChangeToken = ref(0)

const clusterData = computed(() => {
  if (!rawClusterData.value) return null
  return parseClusterTopology(rawClusterData.value)
})

const serverTopoConfig = computed(() => {
  const cd = clusterData.value
  if (!cd || cd.superPods.length === 0) return null
  return parseServerTopoConfig(cd.superPods[0].serverTopoConfig)
})

const selectedTopology = computed(() =>
  topologyList.value.find((t) => t.name === selectedTopoName.value) || null,
)

let listPromise = null

async function ensureTopologyListLoaded() {
  if (topologyList.value.length > 0) return
  if (listPromise) return listPromise

  listPromise = (async () => {
    try {
      loading.value = true
      error.value = ''
      const list = await fetchTopologyList()
      topologyList.value = list
    } catch (e) {
      error.value = e.message || 'Failed to load topology list'
    } finally {
      loading.value = false
    }
  })()

  return listPromise
}

async function selectTopology(name) {
  selectedTopoName.value = name
  loading.value = true
  error.value = ''

  try {
    const raw = await fetchClusterTopology(name)
    rawClusterData.value = raw
    topoChangeToken.value++
  } catch (e) {
    error.value = e.message || 'Failed to load cluster topology'
    rawClusterData.value = null
  } finally {
    loading.value = false
  }
}

function clearTopology() {
  selectedTopoName.value = ''
  rawClusterData.value = null
  error.value = ''
}

export function useTopologyState() {
  return {
    topologyList,
    selectedTopoName,
    clusterData,
    serverTopoConfig,
    selectedTopology,
    loading,
    error,
    topoChangeToken,
    ensureTopologyListLoaded,
    selectTopology,
    clearTopology,
  }
}
