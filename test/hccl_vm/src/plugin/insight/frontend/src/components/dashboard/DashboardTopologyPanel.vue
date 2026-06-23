<script setup>
import { onMounted, computed } from 'vue'
import { Document } from '@element-plus/icons-vue'
import { useTopologyState } from '../../composables/useTopologyState'
import TopoClusterView from '../topology/TopoClusterView.vue'

const {
  topologyList,
  selectedTopoName,
  clusterData,
  serverTopoConfig,
  loading,
  error,
  topoChangeToken,
  ensureTopologyListLoaded,
  selectTopology,
} = useTopologyState()

const hasTopology = computed(() => !!clusterData.value)

onMounted(() => {
  ensureTopologyListLoaded()
})

function handleSelectTopo(name) {
  selectTopology(name)
}
</script>

<template>
  <section class="dashboard-panel dashboard-topology-panel">
    <div class="dashboard-section-title">
      <div>
        <p class="dashboard-eyebrow">Topology</p>
        <h2>Network Topology</h2>
      </div>
      <div class="topo-panel__actions">
        <el-select
          v-model="selectedTopoName"
          placeholder="请选择拓扑配置文件"
          size="default"
          class="topo-panel__select"
          @change="handleSelectTopo"
        >
          <template #prefix>
            <el-icon><Document /></el-icon>
          </template>
          <el-option
            v-for="t in topologyList"
            :key="t.name"
            :label="t.name"
            :value="t.name"
          />
        </el-select>
        <span v-if="loading" class="dashboard-status-pill">Loading...</span>
        <span v-else-if="hasTopology" class="dashboard-status-pill">已加载</span>
        <span v-else class="dashboard-status-pill">未选择</span>
      </div>
    </div>

    <div v-if="error" class="topo-panel__error">
      {{ error }}
    </div>

    <TopoClusterView
      v-if="hasTopology"
      :cluster-data="clusterData"
      :server-topo-config="serverTopoConfig"
      :topo-change-token="topoChangeToken"
    />

    <div v-else class="dashboard-empty-wrap">
      <el-empty :description="loading ? 'Loading...' : '请选择集群拓扑配置'" />
    </div>
  </section>
</template>
