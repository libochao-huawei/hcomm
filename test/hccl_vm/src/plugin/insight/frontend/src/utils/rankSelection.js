function resolveRankCount(detail) {
  const ranks = detail?.memory?.map((item) => item.rank).filter((item) => Number.isInteger(item)) ?? []
  if (ranks.length) {
    return ranks.length
  }

  const rankSize = detail?.opParam?.rank_size
  if (Number.isInteger(rankSize) && rankSize > 0) {
    return rankSize
  }

  return 0
}

export const DEFAULT_SELECTED_RANK_LIMIT = 8

export function buildDefaultSelectedRankKeys(detail, maxDefaultRanks = DEFAULT_SELECTED_RANK_LIMIT) {
  const maxRanks = Math.max(0, Math.floor(Number(maxDefaultRanks) || 0))
  const selectedCount = Math.min(resolveRankCount(detail), maxRanks)
  return Array.from({ length: selectedCount }, (_, index) => `rank-${index}`)
}
