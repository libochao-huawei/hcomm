import { ref } from 'vue'

const pendingMemViewNavigation = ref(null)

export function queueMemViewNavigation(request) {
  pendingMemViewNavigation.value = {
    ...request,
    token: Date.now(),
  }
}

export function clearMemViewNavigation() {
  pendingMemViewNavigation.value = null
}

export function usePageNavigationState() {
  return {
    pendingMemViewNavigation,
    queueMemViewNavigation,
    clearMemViewNavigation,
  }
}
