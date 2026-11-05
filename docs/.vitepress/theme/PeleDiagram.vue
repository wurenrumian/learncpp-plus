<script setup lang="ts">
import { onBeforeUnmount, onMounted, ref } from 'vue'

const props = defineProps<{
  src: string
  caption?: string
}>()

const host = ref<HTMLElement | null>(null)
let mounted: { destroy(): void } | undefined
let observer: IntersectionObserver | undefined
let cancelled = false

function decodeBase64(value: string): string {
  const binary = atob(value)
  const bytes = new Uint8Array(binary.length)
  for (let i = 0; i < binary.length; i += 1) {
    bytes[i] = binary.charCodeAt(i)
  }
  return new TextDecoder().decode(bytes)
}

async function mountDiagram(element: HTMLElement): Promise<void> {
  try {
    const { mountAsync } = await import('pele/lazy')
    if (cancelled) return
    mounted = await mountAsync(element, decodeBase64(props.src), {
      zoom: 'auto',
    })
  } catch (error) {
    cancelled = true
    element.classList.add('pele-error')
    element.textContent = error instanceof Error ? error.message : String(error)
  }
}

onMounted(() => {
  const element = host.value
  if (!element) return

  if (typeof IntersectionObserver === 'undefined') {
    void mountDiagram(element)
    return
  }

  observer = new IntersectionObserver(
    (entries) => {
      if (!entries.some((entry) => entry.isIntersecting)) return
      observer?.disconnect()
      observer = undefined
      void mountDiagram(element)
    },
    { rootMargin: '200px 0px' },
  )
  observer.observe(element)
})

onBeforeUnmount(() => {
  cancelled = true
  observer?.disconnect()
  observer = undefined
  mounted?.destroy()
  mounted = undefined
})
</script>

<template>
  <figure class="pele-figure">
    <div
      ref="host"
      class="pele-canvas"
      role="img"
      :aria-label="caption || 'Mermaid 图'"
    />
    <figcaption v-if="caption">{{ caption }}</figcaption>
  </figure>
</template>
