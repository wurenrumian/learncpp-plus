import DefaultTheme from 'vitepress/theme'
import PeleDiagram from './PeleDiagram.vue'
import './custom.css'
import './pele.css'

export default {
  extends: DefaultTheme,
  enhanceApp({ app }) {
    app.component('PeleDiagram', PeleDiagram)
  },
}
