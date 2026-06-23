<script setup>
import { DataAnalysis, Odometer, Share, WarningFilled } from '@element-plus/icons-vue'

defineProps({
  pages: {
    type: Array,
    required: true,
  },
  activePage: {
    type: String,
    required: true,
  },
})

const emit = defineEmits(['change'])

function handleChange(pageId) {
  emit('change', pageId)
}

const iconMap = {
  dashboard: Odometer,
  'mem-view': Share,
  analytic: WarningFilled,
}
</script>

<template>
  <header class="top-nav">
    <div class="top-nav__brand">
      <div class="top-nav__logo">
        <el-icon><DataAnalysis /></el-icon>
      </div>
      <h1 class="top-nav__title">HVRM Insight</h1>
    </div>

    <nav class="top-nav__tabs" aria-label="Primary">
      <button
        v-for="page in pages"
        :key="page.id"
        type="button"
        class="top-nav__tab"
        :class="{ 'is-active': activePage === page.id }"
        @click="handleChange(page.id)"
      >
        <el-icon><component :is="iconMap[page.id]" /></el-icon>
        <span>{{ page.label }}</span>
      </button>
    </nav>
  </header>
</template>
