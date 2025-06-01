package cframegraph

import (
	callocator "github.com/jurgen-kluft/callocator/package"
	cbase "github.com/jurgen-kluft/cbase/package"
	denv "github.com/jurgen-kluft/ccode/denv"
	cunittest "github.com/jurgen-kluft/cunittest/package"
)

const (
	repo_path = "github.com\\jurgen-kluft"
	repo_name = "cframegraph"
)

func GetPackage() *denv.Package {
	name := repo_name

	// dependencies
	cunittestpkg := cunittest.GetPackage()
	cbasepkg := cbase.GetPackage()
	callocpkg := callocator.GetPackage()

	// main package
	mainpkg := denv.NewPackage(repo_path, repo_name)
	mainpkg.AddPackage(cunittestpkg)
	mainpkg.AddPackage(cbasepkg)
	mainpkg.AddPackage(callocpkg)

	// main library
	mainlib := denv.SetupCppLibProject(mainpkg, name)
	mainlib.AddDependencies(cbasepkg.GetMainLib()...)
	mainlib.AddDependencies(callocpkg.GetMainLib()...)

	// test library
	testlib := denv.SetupCppTestLibProject(mainpkg, name)
	testlib.AddDependencies(cbasepkg.GetTestLib()...)
	testlib.AddDependencies(callocpkg.GetTestLib()...)
	testlib.AddDependencies(cunittestpkg.GetTestLib()...)

	// unittest project
	maintest := denv.SetupCppTestProject(mainpkg, name)
	maintest.AddDependencies(cunittestpkg.GetMainLib()...)
	maintest.AddDependency(testlib)

	mainpkg.AddMainLib(mainlib)
	mainpkg.AddTestLib(testlib)
	mainpkg.AddUnittest(maintest)
	return mainpkg
}
